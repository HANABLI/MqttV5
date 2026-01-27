/**
 * @file MqttClient.cpp
 * @brief This file contains the implementation of the MqttClient class.
 * @note This class is used to connect to an MQTT broker and send/receive messages.
 * @note This class is used to store packets in a circular buffer.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */

#include "MqttV5/MqttClient.hpp"
#include "MqttV5/TimeKeeper.hpp"
#include <Utf8/Utf8.hpp>
#include <TimeTracking/TimeTracking.hpp>
#include <StringUtils/StringUtils.hpp>
#include <string>
#include <memory>
#include <sstream>
#include <mutex>
#include <thread>
#include <map>
#include <set>
#include <functional>
#include <condition_variable>

namespace MqttV5
{
    struct Storage::CircularBufferStore::Impl
    {
        uint32_t read, write;

        const uint32_t bufferSize;

        uint8_t* buffer;

        PacketBookmark* bookmarks;

        uint32_t maxPacketsCount;  // Maximum number of packets that can be stored

        uint32_t FindId(uint32_t id) const {
            for (uint32_t i = 0; i < maxPacketsCount; ++i)
            {
                if (bookmarks[i].id == id)
                { return i; }
            }
            return maxPacketsCount;
        }

        /**
         * get the consumed size of the buffer
         */
        inline uint32_t GetConsumedSize() const {
            return (read <= write) ? write - read : bufferSize - (read - write - 1);
        }

        /**
         * get the free size of the buffer
         */
        inline uint32_t GetFreeSize() const { return bufferSize - GetConsumedSize(); }

        bool SavePacket(uint32_t packetID, uint32_t packetSize, const uint8_t* packet) {
            if (packetSize > bufferSize || GetFreeSize() < packetSize)
            {
                return false;  // Not enough space to store the packet
            }

            uint32_t index = FindId(0);

            if (index == maxPacketsCount)
            { return false; }

            const uint32_t p1 = std::min(packetSize, bufferSize - write + 1);
            const uint32_t p2 = packetSize - p1;

            memcpy(buffer + write, packet, p1);
            memcpy(buffer, packet + p1, p2);

            bookmarks[index].Set(packetID, packetSize, write);
            write = (write + packetSize) & bufferSize;
            return true;
        }

        bool LoadPacket(uint32_t packetID, uint32_t& headerSize, uint8_t*& headerPacket,
                        uint32_t& tailSize, uint8_t*& tailPacket) {
            auto index = FindId(packetID);
            if (index == maxPacketsCount)
                return false;

            auto position = bookmarks[index].pos;
            headerPacket = buffer + position;
            headerSize = std::min(bookmarks[index].size, (bufferSize - position + 1));
            tailSize = bookmarks[index].size - headerSize;
            tailPacket = buffer;
            return true;
        }

        bool RemovePacket(const uint32_t packetID) {
            uint8_t index = FindId(packetID);
            if (index == maxPacketsCount)
            {
                return false;  // Packet not found
            }

            PacketBookmark& packet = bookmarks[index];

            // Adjust read pointer if necessary
            uint32_t position = packet.pos;
            uint32_t size = packet.size;
            uint32_t end = (packet.pos + packet.size) & bufferSize;
            // Clear the bookmark
            packet.Set(0, 0, 0);
            if (position == read)
            {
                // Advence read pointer if packet was at head.
                read = (read + size) & bufferSize;
                return true;
            }

            if (end == write)
            {
                // Rewind write pointer if packet was last written.
                write = position;
                return true;
            }

            // Move memory to fill the gap.
            uint32_t s = bufferSize + 1;
            uint32_t W = (write < position ? write + s : write);
            uint32_t E = (end < position ? end + s : end);

            for (uint32_t j = 0; j < W - E; j++)
            { buffer[(j + position) & bufferSize] = buffer[(j + position + size) & bufferSize]; }

            write = (W - size) & bufferSize;

            // Update metadata for affected packets
            bool searching = true;
            while (searching)
            {
                searching = false;
                for (uint8_t j = 0; j < maxPacketsCount; j++)
                {
                    PacketBookmark& iter = bookmarks[j];
                    if (iter.pos == end)
                    {
                        end = (iter.pos + iter.size) & bufferSize;
                        iter.pos = position;
                        position = (iter.pos + iter.size) & bufferSize;
                        searching = true;
                        break;
                    }
                }
            }

            return true;
        }

        Impl(uint8_t* buffer, uint32_t bufferSize, uint32_t maxPacketsCount,
             PacketBookmark* bookmarks) :
            buffer(buffer),
            read(0),
            write(0),
            bufferSize(bufferSize - 1),
            maxPacketsCount(maxPacketsCount),
            bookmarks(bookmarks) {}
        ~Impl() {
            delete[] buffer;
            delete[] bookmarks;
        }
    };

    bool Storage::CircularBufferStore::StorePacket(uint32_t packetID, uint32_t packetSize,
                                                   const uint8_t* packet) {
        if (!impl_)
        { return false; }  // Not initialized

        return impl_->SavePacket(packetID, packetSize, packet);
    }

    bool Storage::CircularBufferStore::RetrievePacket(uint32_t packetID, uint32_t& headSize,
                                                      uint8_t*& headPacket, uint32_t& tailSize,
                                                      uint8_t*& tailPacket) {
        if (!impl_)
        { return false; }  // Not initialized

        return impl_->LoadPacket(packetID, headSize, headPacket, tailSize, tailPacket);
    }

    bool Storage::CircularBufferStore::RemovePacket(uint32_t packetID) {
        if (!impl_)
        { return false; }  // Not initialized

        return impl_->RemovePacket(packetID);
    }
    Storage::CircularBufferStore::CircularBufferStore(const uint32_t size,
                                                      const uint32_t maxPacketsCount) :
        impl_(std::make_unique<Impl>(new uint8_t[size], size, maxPacketsCount,
                                     new PacketBookmark[maxPacketsCount])) {}
    Storage::CircularBufferStore::~CircularBufferStore() {  // Ensure the implementation is cleaned
                                                            // up
    }

    struct ClockWrapper : public TimeTracking::Clock
    {
        // Properties

        /**
         * This is the timekeeper wrapped by this adapter.
         */
        std::shared_ptr<MqttV5::TimeKeeper> timeKeeper;

        // Methods

        /**
         * Construct a new wrapper for the given timekeeper.
         *
         * @param[in] timekeeper
         *      This is the timekeeper wrapped by this adapter.
         */
        explicit ClockWrapper(std::shared_ptr<MqttV5::TimeKeeper> timeKeeper) :
            timeKeeper(timeKeeper) {}

        // Timekeeping::Clock

        double GetCurrentTime() override { return timeKeeper->GetCurrentTime(); }
    };
    /**
     * This holds onto all the information that a client has about
     * a connection to a server.
     */
    struct ConnectionState
    {
        /**
         * This is the current state of the connection.
         * It is used to track the connection state of the client.
         */
        std::shared_ptr<Connection> connection;

        /**
         * This is the current transaction that is being processed.
         * It is used to track the current transaction state of the client.
         */
        std::weak_ptr<struct TransactionImpl> currentTransaction;

        /**
         * Receive buffer on this connection
         */
        std::vector<uint8_t> rxBuffer;

        /**
         * This indicates if the connection is broken.
         * If true, the client will attempt to reconnect to the broker.
         */
        bool broken = false;  // This is used to track if the connection is broken
        /**
         * This is used to synchronize access to the object.
         */
        std::recursive_mutex mutex;

        /**
         * This is the function to call to schedule the connection to
         * be dropped if it remains inactive for too long past the current
         * point in time.
         */
        std::function<void()> setInactivityTimeout;

        /**
         * This is the token representing the sheduled callback for timing
         * out the connection for inactivity.
         */
        int inactivityCallbackToken = 0;
    };

    /**
     * This is a client transaction structure that contains the additional
     * properties and methods required by the client's implementation.
     */
    struct TransactionImpl : public MqttClient::Transaction
    {
        /**
         * This is the token representing the scheduled callback for timing
         * out the transaction.
         */
        int receiveTimeoutToken = 0;

        /**
         * This is the state of the connection to the server
         * used by this transaction.
         */
        std::shared_ptr<ConnectionState> connectionState;

        /**
         * This is a pointer to the client that handle the transaction
         */
        std::weak_ptr<MqttClient::Impl> impl_;

        /**
         * The completion delegate function
         */
        std::function<void(std::vector<ReasonCode>& reasons)> completionDelegate;

        /**
         * This flag indicates whether or not the connection used to
         * communicate with the server should be kept open after the
         * request, possibly to be reused in subsequent requests.
         */
        bool persistConnection = false;

        /**
         * This flag indicates whether or not the transaction is complete.
         */
        bool complete = false;

        /**
         * This is a mutex used to synchronize access to the object.
         */
        std::recursive_mutex mutex;

        /**
         * This is used to wait for various object state changes.
         */
        std::condition_variable_any stateChange;

        /**
         * This is a vector of reason received on response to a transaction.
         */
        std::vector<Storage::ReasonCode> reasons;

        using State = MqttClient::Transaction::State;

        bool AwaitCompletion(const std::chrono::milliseconds& relativeTime) override;

        void AwaitCompletion() override;

        void SetCompletionDelegate(
            std::function<void(std::vector<ReasonCode>& reasons)> cb) override;

        /**
         *
         */
        void MarkComplete(State reasons, double now);

        // déclarations seulement - supprimer les bodies inline
        void HandleConnAck(const uint8_t* packetPtr, uint32_t packetSize, double now);
        void HandlePingResp(const uint8_t* packetPtr, uint32_t packetSize, double now);
        void HandleSubAck(const uint8_t* packetPtr, uint32_t packetSize, double now);
        void HandleUnSubAck(const uint8_t* packetPtr, uint32_t packetSize, double now);
        void HandlePublish(const uint8_t* packetPtr, uint32_t packetSize, double now);
        void HandlePubAck(const uint8_t* packetPtr, uint32_t packetSize, double now);
        void HandlePubRec(const uint8_t* packetPtr, uint32_t packetSize, double now);
        void HandlePubComp(const uint8_t* packetPtr, uint32_t packetSize, double now);
        void HandlePubRel(const uint8_t* packetPtr, uint32_t packetSize, double now);
        bool DataReceived(const std::vector<uint8_t>& data, double now);
        void ConnectionBroken(double now);
    };

    class ClientConnectionPool
    {
        // Types
    public:
        typedef std::set<std::shared_ptr<ConnectionState>> ConnectionSet;
        // Public Methods
    public:
        /**
         * This method is attempts to find a connection in the pool that isn't
         * broken and doesn't have any transaction using it. If one is found,
         * the given transaction is assigned to it, and the connection is
         * returned. Otherwise, nullptr is returned.
         *
         * @param[in] brokerId
         *      This is a unique identifier of the broker for which a connection
         *      is established, in the form host port.
         *
         * @param[in] transaction
         *      This is the transaction to assigned to free connection.
         *
         * @param[in] time
         *      This is the time in which the transaction began.
         * @param[in] scheduler
         *      This is used to schedule the work to be done without having
         *      to poll the timekeeper.
         * @return
         *      If the given transaction was successfully assigned to a connection,
         *      the connection is returned.
         * @retval
         *      A nullptr is returned if no free connection could be found.
         */
        std::shared_ptr<ConnectionState> AttachTransaction(
            const std::string& brokerId, std::shared_ptr<TransactionImpl> transaction, double time,
            TimeTracking::Scheduler& scheduler) {
            std::lock_guard<decltype(mutex)> poolLock(mutex);
            const auto connectionEntry = connections.find(brokerId);
            if (connectionEntry != connections.end())
            {
                for (auto& connection : connectionEntry->second)
                {
                    if (!connection->broken && (connection->currentTransaction.lock() == nullptr))
                    {
                        if (connection->inactivityCallbackToken != 0)
                        {
                            scheduler.Cancel(connection->inactivityCallbackToken);
                            connection->inactivityCallbackToken = 0;
                        }
                        connection->currentTransaction = transaction;
                        return connection;
                    }
                }
            }
            return nullptr;
        }

        /**
         * This adds the given connection to the pool, if it isn't broken.
         *
         * @param brokerId
         *      This is the identifier of the broker to which the connection
         *      is established.
         * @param connection
         *      This is the connection to add to the pool.
         */
        void AddConnection(const std::string& brokerId,
                           std::shared_ptr<ConnectionState> connection) {
            std::lock_guard<decltype(mutex)> poolLock(mutex);
            std::lock_guard<decltype(connection->mutex)> connectionLock(connection->mutex);
            if (!connection->broken)
            { (void)connections[brokerId].insert(connection); }
        }

        /**
         * This removes the given connection from the pool.
         *
         * @param brokerId
         *      This is the identifier of the connection to drop from the pool.
         *
         * @param connection
         *      This is the connection to drop from the pool.
         */
        void DropConnection(const std::string& brokenId,
                            std::shared_ptr<ConnectionState> connection) {
            std::lock_guard<decltype(mutex)> poolLock(mutex);
            auto& connectionPool = connections[brokenId];
            (void)connectionPool.erase(connection);
            if (connectionPool.empty())
            { (void)connections.erase(brokenId); }
        }

        // Properties
    private:
        /**
         * This is the mutex used to synchronize access to the object.
         */
        std::mutex mutex;

        /**
         * This is the pool of connections, organized by identifier (host::port).
         */
        std::map<std::string, ConnectionSet> connections;
    };

    template <typename T>
    struct ImplBase
    {
        /**
         * The DER encoded certificate (if provided).
         */
        const Storage::DynamicBinaryDataView* brokerCert;
        /**
         * The client unique identifier.
         */
        const char* clientID;
        /**
         * The last communication time in second.
         */
        uint32_t lastCommunication;
        /**
         * The keep alive delay in seconds.
         */
        uint16_t keepAlive;
        /**
         * The publish current default identifier allocator.
         */
        uint16_t publishCurrenId;
        /**
         * The last unsubscribe identifier (needed when Mqtt use unsubscribe is activated).
         */
        uint16_t lastUnsubscribeId;
        /**
         * The last unsubscribe error code id (needed when Mqtt use unsubscribe is activated).
         */
        MqttClient::Transaction::State lastUnsubscribeError;
        /**
         * The storage interface (use when Mqtt QoS Support Level Parameter is activated).
         */
        Storage::PacketStore* storage;
        /**
         * Message callback function
         */
        Storage::MessageReceived* cb;
        /**
         * The maximum packet size the server is willing to accept.
         */
        uint32_t maxPacketSize{65535};
        /**
         * The available data in the buffer.
         */
        uint32_t available;
        /**
         * The receiving buffer.
         */
        Buffer buffers;
        /**
         * Client State
         */
        MqttClient::ClientState state;
        /**
         * The receiving VBInt size for the packet header.
         */
        uint16_t packetExpectedVBSize;

        explicit ImplBase(const char* clientID, Storage::MessageReceived* callback,
                          const Storage::DynamicBinaryDataView* brokerCert,
                          Storage::PacketStore* storage) :
            brokerCert(brokerCert),
            clientID(clientID),
            cb(callback),
            lastCommunication(0),
            publishCurrenId(0),
            keepAlive(300),
            lastUnsubscribeId(0),
            lastUnsubscribeError(MqttClient::Transaction::State::WaitingForResult),
            storage(storage
                        ? storage
                        : (Storage::PacketStore*)new Storage::CircularBufferStore(1u << 16, 256)),
            maxPacketSize(max(callback ? callback->maxPacketSize() : 1024u, 8u)),
            available(0),
            buffers(max(callback->maxPacketSize(), (uint32_t)8UL),
                    min(callback->maxUnAckedPackets(), (uint32_t)127UL)),
            packetExpectedVBSize(Common::VBInt(max(callback->maxPacketSize(), (uint32_t)8UL)).size),
            state(MqttClient::ClientState::Unknown) {}
        ~ImplBase() { delete storage; }

        uint16_t nextPacketId() {
            if (++publishCurrenId == 0)
                publishCurrenId = 1;
            return publishCurrenId;
        }

        bool saveQoS(uint32_t id, const std::vector<uint8_t>& bytes) {
            return storage && storage->StorePacket(id, (uint32_t)bytes.size(), bytes.data());
        }

        bool loadQos(uint32_t id, uint32_t& h, uint8_t*& hp, uint32_t& t, uint8_t*& tp) {
            return storage && storage->RetrievePacket(id, h, hp, t, tp);
        }

        bool removeQos(uint32_t id) { return storage && storage->RemovePacket(id); }
    };

    struct MqttClient::Impl : public ImplBase<MqttClient::Impl>
    {
        /**
         * This is the type used to handle collections of transactions that the
         * client knows about.  The keys are arbitrary (but unique for the
         * client instance) identifiers used to help select transactions from
         * collections.
         */
        typedef std::map<unsigned int, std::weak_ptr<TransactionImpl>> TransactionCollection;

        /**
         * This is the delegate to generate diagnostic messages
         * for the client.
         */
        SystemUtils::DiagnosticsSender diagnosticSender;

        // This is used to track if the client is mobilized
        bool mobilized = false;

        // This is the transport layer implementation to use.
        std::shared_ptr<ClientTransportLayer> transport;

        // This is the connection state
        std::weak_ptr<ConnectionState> connectionState;

        // This is the object used to track time in the client.
        std::shared_ptr<TimeKeeper> timeKeeper;
        /**
         * This is used to schedule work to be done without having to
         * poll the timekeeper.
         */
        std::shared_ptr<TimeTracking::Scheduler> scheduler;

        /**
         * This is used to hold onto persistent connections to servers.
         */
        std::shared_ptr<ClientConnectionPool> persistentConnections =
            std::make_shared<ClientConnectionPool>();

        /**
         * This is the collection of client transactions currently active,
         * keyed by transaction ID.
         */
        TransactionCollection activeTransactions;

        /**
         * This is the ID to assign to the next transaction.
         */
        unsigned int nextTransactionId = 1;

        /**
         * This is the mutex used to synchronize access to the object.
         */
        std::recursive_mutex mutex;

        // Options
        MqttOptions options;

        // This is the amount of time after a request is made
        // of a server, before the transaction is considered timed out
        // if no part of a response has been received.
        double requestTimeoutSeconds = DEFAULT_REQUEST_TIMEOUT_SECONDS;

        // This is the amount of time, after a transaction is completed,
        // that a persistent connection is closed if another transaction
        // does not reuse the connection.
        double inactivityInterval = DEFAULT_INACTIVITY_INTERVAL_SECONDS;

        // Lifecycle management

        ~Impl() noexcept = default;
        Impl(const Impl&) = delete;
        Impl(Impl&&) noexcept = delete;
        Impl& operator=(const Impl&) = delete;
        Impl& operator=(Impl&&) noexcept = delete;

        // Methods

        /**
         * This is the constructor for the structure.
         */
        Impl(const char* clientID, Storage::MessageReceived* callback,
             const Storage::DynamicBinaryDataView* brokerCert, Storage::PacketStore* storage) :
            ImplBase(clientID, callback, brokerCert, storage),
            diagnosticSender("MqttV5::MqttClient") {}

        void retransmitPending(const std::shared_ptr<ConnectionState>& connectionState) {
            for (uint8_t i = 0; i < buffers.end(); i++)
            {
                uint32_t raw = buffers.packetID(i);
                if (!Buffer::isSending(raw))
                    continue;
                uint16_t pid = (uint16_t)(raw & 0x1FFFF);
                uint32_t hs = 0, ts = 0;
                uint8_t* h = nullptr;
                uint8_t* t = nullptr;
                if (loadQos(pid, hs, h, ts, t))
                {
                    if (hs && h)
                    { connectionState->connection->SendData(std::vector<uint8_t>(h, h + hs)); }
                    if (ts && t)
                    { connectionState->connection->SendData(std::vector<uint8_t>(t, t + ts)); }
                }
            }
        }

        auto CreateConnection(std::shared_ptr<TransactionImpl> transaction,
                              const std::string& brokerId, const std::string& scheme,
                              const std::string& brokerHost, uint16_t port)
            -> std::shared_ptr<ConnectionState> {
            // TODO either get a connection from a pool of connection or a map of connection neither
            // create a new one.

            const auto connectionState = std::make_shared<ConnectionState>();
            std::lock_guard<decltype(connectionState->mutex)> lock(connectionState->mutex);
            std::weak_ptr<ConnectionState> connectionStateWeak(connectionState);
            std::weak_ptr<ClientConnectionPool> persistentConnectionsWeak(persistentConnections);
            auto timeKeeperRef = timeKeeper;
            connectionState->connection = transport->Connect(
                scheme, brokerHost, port,
                [brokerId, connectionStateWeak, persistentConnectionsWeak,
                 timeKeeperRef](const std::vector<uint8_t>& data)
                {
                    const auto connectionState = connectionStateWeak.lock();
                    if (connectionState == nullptr)
                    { return; }

                    std::shared_ptr<TransactionImpl> transaction;
                    {
                        std::lock_guard<decltype(connectionState->mutex)> lock(
                            connectionState->mutex);
                        transaction = connectionState->currentTransaction.lock();
                    }

                    if (transaction == nullptr)
                    { return; }

                    // Pour MQTT : on traite les données mais on NE DROPPE PAS la connexion.
                    // La connexion doit rester ouverte tant que le client est actif.
                    (void)transaction->DataReceived(data, timeKeeperRef->GetCurrentTime());
                },
                [brokerId, connectionStateWeak, persistentConnectionsWeak, timeKeeperRef](bool)
                {
                    const auto connectionState = connectionStateWeak.lock();
                    if (connectionState == nullptr)
                    { return; }
                    std::shared_ptr<TransactionImpl> transaction;
                    {
                        std::lock_guard<decltype(connectionState->mutex)> lock(
                            connectionState->mutex);
                        transaction = connectionState->currentTransaction.lock();
                        connectionState->broken = true;
                    }
                    if (transaction == nullptr)
                    { return; }
                    transaction->ConnectionBroken(timeKeeperRef->GetCurrentTime());
                    const auto persistentConnection = persistentConnectionsWeak.lock();
                    if (persistentConnection != nullptr)
                    { persistentConnection->DropConnection(brokerId, connectionState); }
                });
            connectionState->currentTransaction = transaction;
            return connectionState;
        }

        /**
         * This method adds the given transaction to the collection of
         * transactions active for the client.
         *
         * @param[in] transaction
         *     This is the transaction to add to the collection of transactions
         *     active for the client.
         */
        void AddTransaction(std::shared_ptr<TransactionImpl> transaction) {
            std::lock_guard<decltype(mutex)> lock(mutex);
            activeTransactions[nextTransactionId++] = transaction;
        }

        /**
         * This method remove the given transaction from the collection of transactions.
         *
         * @param[in] transaction
         *     This is the transaction to remove from the collection of transactions
         *     active for the client.
         */
        void RemoveTransaction(std::shared_ptr<TransactionImpl> transaction) {
            std::lock_guard<decltype(mutex)> lock(mutex);
            for (auto it = activeTransactions.begin(); it != activeTransactions.end(); ++it)
            {
                if (it->second.lock() == transaction)
                {
                    activeTransactions.erase(it);
                    break;
                }
            }
        }
    };

    bool TransactionImpl::AwaitCompletion(const std::chrono::milliseconds& relativeTime) {
        std::unique_lock<decltype(mutex)> lock(mutex);
        return stateChange.wait_for(lock, relativeTime, [this] { return complete; });
    }

    void TransactionImpl::AwaitCompletion() {
        std::unique_lock<decltype(mutex)> lock(mutex);
        return stateChange.wait(lock, [this] { return complete; });
    }

    void TransactionImpl::SetCompletionDelegate(std::function<void(std::vector<ReasonCode>&)> cb) {
        std::unique_lock<decltype(mutex)> lock(mutex);
        this->completionDelegate = cb;
        const bool wasComplete = complete;
        lock.unlock();
        if (wasComplete)
        { cb(reasons); }
    }

    void TransactionImpl::MarkComplete(State state, double due) {
        std::lock_guard<decltype(mutex)> lock(mutex);
        if (complete)
        { return; }

        transactionState = state;
        complete = true;

        // Gestion de la connexion :
        // Pour MQTT on garde la connexion TCP persistante.
        // On ne ferme la connexion que si elle a expiré (timeout).
        if (connectionState && connectionState->connection != nullptr)
        {
            if (state == Transaction::State::TimedOut)
            {
                // En cas de timeout, on considère la connexion comme HS.
                connectionState->connection->Break(false);
                connectionState->broken = true;
            }

            connectionState->currentTransaction.reset();

            if (connectionState->setInactivityTimeout != nullptr)
            { connectionState->setInactivityTimeout(); }
        }

        auto cb = completionDelegate;
        stateChange.notify_all();
        if (cb != nullptr)
        {
            cb(reasons);
            reasons.erase(reasons.begin(), reasons.end());
        }
        impl_.lock()->RemoveTransaction(connectionState->currentTransaction.lock());
    }

    void TransactionImpl::HandleUnSubAck(const uint8_t* packetPtr, uint32_t packetSize,
                                         double now) {
        UnsubAckPacket unSubAck;
        auto desSize = unSubAck.deserialize(packetPtr, packetSize);
        if (!unSubAck.checkImpl() || (desSize != packetSize))
        {
            transactionState = State::ShunkedPacket;
            return;
        }
        auto impl = impl_.lock();
        if (unSubAck.fixedVariableHeader.packetID != packetID)
        {
            if (!connectionState->broken)
            { connectionState->connection->Break(true); }
            if (impl && impl->cb && connectionState->broken)
            { (void)impl->cb->onConnectionLost(State::NetworkError); }
        }

        State allSuccess = State::Success;
        for (uint32_t i = 0; i < unSubAck.payload.dataSize; ++i)
        {
            const uint8_t raw = unSubAck.payload.data[i];
            if (raw >= 0x80)
            { allSuccess = State::NetworkError; }
            reasons.push_back(static_cast<Storage::ReasonCode>(raw));
        }

        MarkComplete(allSuccess, now);
    }

    void TransactionImpl::HandleSubAck(const uint8_t* packetPtr, uint32_t packetSize, double now) {
        SubAckPacket subAck;
        auto desSize = subAck.deserialize(packetPtr, packetSize);
        if (!subAck.checkImpl() || (desSize != packetSize))
        {
            transactionState = State::ShunkedPacket;
            return;
        }
        auto impl = impl_.lock();
        if (subAck.fixedVariableHeader.packetID != packetID)
        {
            if (!connectionState->broken)
            { connectionState->connection->Break(true); }
            if (impl && impl->cb && connectionState->broken)
            { (void)impl->cb->onConnectionLost(State::NetworkError); }
        }

        State allSuccess = State::Success;
        for (uint32_t i = 0; i < subAck.payload.dataSize; ++i)
        {
            const uint8_t raw = subAck.payload.data[i];
            if (raw >= 0x80)
            { allSuccess = State::NetworkError; }
            reasons.push_back(static_cast<Storage::ReasonCode>(raw));
        }

        MarkComplete(allSuccess, now);
    }

    void TransactionImpl::HandleConnAck(const uint8_t* packetPtr, uint32_t packetSize, double now) {
        ConnAckPacket receivedAck;
        uint32_t desSize = receivedAck.deserialize(packetPtr, packetSize);
        if (!receivedAck.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket, now);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedAck.props.checkPropertiesFor(ControlPacketType::CONNACK))
            {
                MarkComplete(State::BadProperties, now);
                return;
            }
        }
        reasons.push_back((ReasonCode)receivedAck.fixedVariableHeader.reasonCode);

        if (reasons.back() == Storage::ReasonCode::Success)
        {
            if (connectionState && impl)
            {
                impl->retransmitPending(connectionState);
                MarkComplete(State::Success, now);
            }
        } else
        {
            MarkComplete(State::NetworkError, now);
            if (impl && impl->cb && connectionState->broken)
            { (void)impl->cb->onConnectionLost(State::NetworkError); }
        }
    }

    void TransactionImpl::HandlePublish(const uint8_t* packetPtr, uint32_t packetSize, double now) {
        PublishPacket pubPacket;
        auto desSize = pubPacket.deserialize(packetPtr, packetSize);
        if (!pubPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket, now);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!pubPacket.props.checkPropertiesFor(ControlPacketType::PUBLISH))
            {
                MarkComplete(State::BadProperties, now);
                return;
            }
        }
        auto qos = (QoSDelivery)pubPacket.header.getQoS();
        auto packetID = pubPacket.fixedVariableHeader.packetID;

        // auto prop = pubPacket.props;
        DynamicBinaryDataView dynamaicPayload(pubPacket.payload.data, pubPacket.payload.dataSize);
        DynamicStringView topicView(
            reinterpret_cast<const char*>(pubPacket.fixedVariableHeader.topicName.data),
            pubPacket.fixedVariableHeader.topicName.size);
        impl->cb->onMessageReceived(topicView, dynamaicPayload, packetID);
        if (qos == QoSDelivery::AtLeastOne)
        {
            auto pubAckPacket =
                PacketsBuilder::buildPubAckPacket(packetID, ReasonCode::Success, nullptr);
            auto pubAckPacketSize = pubAckPacket->computePacketSize(true);
            std::vector<uint8_t> pubAckBuff;
            pubAckBuff.reserve(static_cast<size_t>(pubAckPacketSize));
            auto pubAckBuffSize = pubAckPacket->serialize(pubAckBuff.data());
            if (pubAckBuffSize != pubAckPacketSize)
            { pubAckBuff.resize(pubAckBuffSize); }
            connectionState->connection->SendData(pubAckBuff);
            MarkComplete(State::Success, now);
            return;
        } else if (qos == QoSDelivery::ExactlyOne)
        {
            auto pubRecPacket =
                PacketsBuilder::buildPubRecPacket(packetID, ReasonCode::Success, nullptr);
            auto pubRecPacketSize = pubRecPacket->computePacketSize(true);
            std::vector<uint8_t> pubRecBuff;
            pubRecBuff.resize((size_t)pubRecPacketSize);
            auto pubRecBuffSize = pubRecPacket->serialize(pubRecBuff.data());
            if (pubRecPacketSize != pubRecBuffSize)
            { pubRecBuff.resize(pubRecBuffSize); }
            connectionState->connection->SendData(pubRecBuff);
            return;
        }
        MarkComplete(State::Success, now);
    }

    void TransactionImpl::HandlePubAck(const uint8_t* packetPtr, uint32_t packetSize, double now) {
        PubAckPacket receivedPacket;
        auto desSize = receivedPacket.deserialize(packetPtr, packetSize);
        if (!receivedPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket, now);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedPacket.props.checkPropertiesFor(ControlPacketType::PUBACK))
            { MarkComplete(State::BadProperties, now); }
        }
        auto id = receivedPacket.fixedVariableHeader.packetID;

        auto i = impl->buffers.findID(id);
        if (i < impl->buffers.end())
        {
            impl->buffers.releaseID(id);
            (void)impl->removeQos(id);
        } else
        {
            MarkComplete(State::StorageError, now);
            return;
        }
        reasons.push_back((ReasonCode)receivedPacket.fixedVariableHeader.reasonCode);
        if (reasons.back() == Storage::ReasonCode::Success)
        {
            MarkComplete(State::Success, now);
        } else
        {
            MarkComplete(State::NetworkError, now);
            if (impl && impl->cb && connectionState->broken)
            { (void)impl->cb->onConnectionLost(State::NetworkError); }
        }
    }

    void TransactionImpl::HandlePubRec(const uint8_t* packetPtr, uint32_t packetSize, double now) {
        PubAckPacket receivedPacket;
        auto desSize = receivedPacket.deserialize(packetPtr, packetSize);
        if (!receivedPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket, now);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedPacket.props.checkPropertiesFor(ControlPacketType::PUBREC))
            {
                MarkComplete(State::BadProperties, now);
                return;
            }
        }
        auto id = receivedPacket.fixedVariableHeader.packetID;
        auto txReason = ReasonCode::Success;
        auto i = impl->buffers.findID(id);
        if (i < impl->buffers.end())
        {
            // Marquer passage à l'étage 2 Qos2
            impl->buffers.avanceQoS2(id);

        } else
        { txReason = ReasonCode::PacketIdentifierNotFound; }
        auto packet = PacketsBuilder::buildPubRelPacket(id, txReason, nullptr);
        auto size = packet->computePacketSize(true);
        std::vector<uint8_t> encodedConnect;
        encodedConnect.reserve((size_t)size);
        auto serSize = packet->serialize(encodedConnect.data());
        if (size != serSize)
        { encodedConnect.resize(serSize); }
        impl->connectionState.lock()->connection->SendData(encodedConnect);
    }

    void TransactionImpl::HandlePubComp(const uint8_t* packetPtr, uint32_t packetSize, double now) {
        PubCompPacket receivedPacket;
        auto desSize = receivedPacket.deserialize(packetPtr, packetSize);
        if (!receivedPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket, now);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedPacket.props.checkPropertiesFor(ControlPacketType::PUBCOMP))
            {
                MarkComplete(State::BadProperties, now);
                return;
            }
        }
        auto id = receivedPacket.fixedVariableHeader.packetID;
        auto rxReason = receivedPacket.fixedVariableHeader.reasonCode;
        reasons.push_back((ReasonCode)rxReason);
        auto allSuccess = State::Success;
        if (!impl->buffers.releaseID(id))
        { allSuccess = State::BadParameter; }
        if (allSuccess == State::Success)
        {
            if (!impl->removeQos(id))
            { allSuccess = State::StorageError; }
        }

        MarkComplete(allSuccess, now);
    }

    void TransactionImpl::HandlePubRel(const uint8_t* packetPtr, uint32_t packetSize, double now) {
        PubRelPacket receivedPacket;
        auto desSize = receivedPacket.deserialize(packetPtr, packetSize);
        if (!receivedPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket, now);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedPacket.props.checkPropertiesFor(ControlPacketType::PUBREL))
            {
                MarkComplete(State::BadProperties, now);
                return;
            }
        }

        auto pubCompPacket =
            PacketsBuilder::buildPubCompPacket(packetID, ReasonCode::Success, nullptr);
        auto pubCompPacketSize = pubCompPacket->computePacketSize(true);
        std::vector<uint8_t> pubCompBuff;
        pubCompBuff.reserve(static_cast<size_t>(pubCompPacketSize));
        auto pubCompBuffSize = pubCompPacket->serialize(pubCompBuff.data());
        if (pubCompBuffSize != pubCompPacketSize)
        { pubCompBuff.resize(pubCompBuffSize); }
        connectionState->connection->SendData(pubCompBuff);
    }

    void TransactionImpl::HandlePingResp(const uint8_t* packetPtr, uint32_t packetSize,
                                         double now) {
        PingRespPacket receivedPacket;
        auto desSize = receivedPacket.deserialize(packetPtr, packetSize);
        if (!receivedPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket, now);
            return;
        }
        auto impl = impl_.lock();

        MarkComplete(State::Success, now);
    }

    bool TransactionImpl::DataReceived(const std::vector<uint8_t>& data, double now) {
        std::unique_lock<decltype(mutex)> lock(mutex);
        auto connectionStateRef = connectionState;
        if (complete)
        { return false; }
        auto& rxBuf = connectionStateRef->rxBuffer;
        rxBuf.insert(rxBuf.end(), data.begin(), data.end());

        size_t offset = 0;

        while (true)
        {
            size_t available = rxBuf.size() - offset;
            if (available < 2)
                break;
            ControlPacketType type;
            FixedHeaderBase header;
            header.typeandFlags = rxBuf[offset];
            type = (ControlPacketType)(uint8_t)header.getType();
            Mqtt_V5::VBInt len;
            uint32_t r = len.readFrom(rxBuf.data() + 1, (uint32_t)available - 1);
            if (r == MqttV5::Mqtt_V5::BadData || r == Mqtt_V5::NotEnoughData)
                break;
            uint32_t remainingLength = len;
            uint32_t totalPacketSize = remainingLength + len.getSerializedSize() + 1;
            if (available < totalPacketSize)
            { break; }

            const uint8_t* packetPtr = rxBuf.data() + offset;
            uint32_t packetSize = totalPacketSize;

            switch (type)
            {
            case MqttV5::ControlPacketType::CONNACK:
                HandleConnAck(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::PUBLISH:
                HandlePublish(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::PUBACK:
                HandlePubAck(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::SUBSCRIBE:
                // HandleSubscribe(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::SUBACK:
                HandleSubAck(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::UNSUBACK:
                HandleUnSubAck(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::PINGRESP:
                HandlePingResp(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::PINGREQ:
                // HandlePingReq(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::PUBREL:
                HandlePubRel(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::PUBREC:
                HandlePubRec(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::PUBCOMP:
                HandlePubComp(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::AUTH:
                // HandleAuth(packetPtr, packetSize, now);
                break;
            case MqttV5::ControlPacketType::DISCONNECT:
                // HandleDisconnect(packetPtr, packetSize, now);
                break;
            default:
                break;
            }

            offset += totalPacketSize;
        }
        lock.unlock();
        if (offset > 0)
        {
            rxBuf.erase(rxBuf.begin(), rxBuf.begin() + offset);
            return true;
        } else
        { return false; }
    }

    void TransactionImpl::ConnectionBroken(double now) {
        std::unique_lock<decltype(mutex)> lock(mutex);
        if (complete)
        { return; }

        lock.unlock();
        MarkComplete(State::NetworkError, now);
    }

    /**
     *
     *
     */

    void MqttClient::Mobilize(const MqttMobilizationDependencies& deps) {
        if (impl_->mobilized)
        { return; }
        impl_->transport = deps.transport;
        impl_->inactivityInterval = deps.inactivityInterval;
        impl_->timeKeeper = deps.timeKeeper;
        impl_->requestTimeoutSeconds = deps.requestTimeoutSeconds;
        impl_->options = deps.options;
        impl_->scheduler.reset(new TimeTracking::Scheduler);
        impl_->scheduler->SetClock(std::make_shared<ClockWrapper>(impl_->timeKeeper));
        impl_->mobilized = true;
    }

    void MqttClient::Demobilize() {
        impl_->scheduler = nullptr;
        impl_->timeKeeper = nullptr;
        impl_->transport = nullptr;
        impl_->mobilized = false;
    }

    SystemUtils::DiagnosticsSender::UnsubscribeDelegate MqttClient::SubscribeToDiagnostics(
        SystemUtils::DiagnosticsSender::DiagnosticMessageDelegate delegate, size_t minLevel) {
        return impl_->diagnosticSender.SubscribeToDiagnostics(delegate, minLevel);
    }

    TimeTracking::Scheduler& MqttClient::GetScheduler() { return *impl_->scheduler; }

    auto MqttClient::ConnectTo(const std::string& brokerId, const std::string& brokerHost,
                               const uint16_t port, bool useTLS, const bool cleanSession,
                               const uint16_t keepAlive, const char* userName,
                               const DynamicBinaryData* password, WillMessage* willMessage,
                               const QoSDelivery willQoS, const bool willRetain,
                               Properties* properties) -> std::shared_ptr<MqttClient::Transaction> {
        if (!impl_->transport || !impl_->mobilized)
        {
            impl_->diagnosticSender.SendDiagnosticInformationString(
                0, "MqttClient::ConnectTo called before Server Mobilization");
            return nullptr;
        }
        const auto transaction = std::make_shared<TransactionImpl>();
        std::weak_ptr<TransactionImpl> transactionWeak(transaction);
        const auto timeKeeperCopy = impl_->timeKeeper;
        transaction->receiveTimeoutToken = impl_->scheduler->Schedule(
            [transactionWeak, timeKeeperCopy]
            {
                auto transaction = transactionWeak.lock();
                if (transaction != nullptr)
                {
                    (void)transaction->MarkComplete(Transaction::State::TimedOut,
                                                    timeKeeperCopy->GetCurrentTime());
                }
            },
            impl_->timeKeeper->GetCurrentTime() + impl_->requestTimeoutSeconds);
        const auto now = impl_->timeKeeper->GetCurrentTime();
        auto connectionState = impl_->persistentConnections->AttachTransaction(
            brokerId, transaction, now, *impl_->scheduler);
        std::string scheme = useTLS ? "mqtts" : "mqtt";
        if (connectionState == nullptr)
        {
            connectionState =
                impl_->CreateConnection(transaction, brokerId, scheme, brokerHost, port);
        }

        if ((connectionState->connection != nullptr))
        {
            impl_->persistentConnections->AddConnection(brokerId, connectionState);
            std::weak_ptr<Impl> implWeak(impl_);
            std::weak_ptr<ConnectionState> connectionStateWeak(connectionState);
            connectionState->setInactivityTimeout = [implWeak, brokerId, connectionStateWeak]()
            {
                auto impl = implWeak.lock();
                auto connectionState = connectionStateWeak.lock();
                if ((impl == nullptr) || (connectionState == nullptr))
                { return; }
                connectionState->inactivityCallbackToken = impl->scheduler->Schedule(
                    [implWeak, brokerId, connectionStateWeak]
                    {
                        auto impl = implWeak.lock();
                        auto connectionState = connectionStateWeak.lock();
                        if ((impl == nullptr) || (connectionState == nullptr))
                        { return; }
                        std::lock_guard<decltype(connectionState->mutex)> connectionLock(
                            connectionState->mutex);
                        if (connectionState->currentTransaction.lock() != nullptr)
                        { return; }
                        // connectionState->broken = true;
                        // impl->persistentConnections->DropConnection(brokerId, connectionState);
                    },
                    impl->timeKeeper->GetCurrentTime() + impl->inactivityInterval);
            };
        } else
        { impl_->persistentConnections->DropConnection(brokerId, connectionState); }

        impl_->state = MqttClient::ClientState::Connecting;
        if (connectionState->broken)
        {
            impl_->diagnosticSender.SendDiagnosticInformationFormatted(
                0, "Connection: State %d", Transaction::State::NetworkError);
        } else
        { impl_->connectionState = connectionState; }
        // TODO Deal with time managment
        impl_->keepAlive =
            (keepAlive + (keepAlive / 2)) /
            2;  // Make it 75% of what's given so we always wake up before doom's clock

        if (properties != nullptr)
        {
            if (!properties->getProperty(PropertyId::PacketSizeMax))
            {
                auto packetSizeMax = MaximumPacketSize_prop::create(impl_->maxPacketSize);
                properties->addProperty(packetSizeMax);
            }
            if (!properties->getProperty(PropertyId::ReceiveMax))
            {
                auto receiveMax = ReceiveMaximum_prop::create(8UL / 3);
                properties->addProperty(receiveMax);
            }
        }

        auto packet = PacketsBuilder::buildConnectPacket(
            impl_->clientID, userName, password, cleanSession, impl_->keepAlive, willMessage,
            willQoS, willRetain, properties);
        // TODO check validation in the PacketsBuilder module
        // if (impl_->options.avoidValidation)
        // {
        //     if (!packet->getProperties().checkPropertiesFor(CONNECT))
        //         impl_->diagnosticSender.SendDiagnosticInformationFormatted(
        //             0, "Check properties for Connection: State %d",
        //             Transaction::State::BadProperties);
        // }
        std::vector<uint8_t> encodedConnect;
        auto size = packet->computePacketSize(true);

        encodedConnect.reserve(static_cast<size_t>(size));
        auto packetSize = packet->serialize(encodedConnect.data());

        transaction->connectionState = connectionState;
        transaction->impl_ = impl_;
        if (size != packetSize)
        { encodedConnect.resize(packetSize); }
        connectionState->connection->SendData(encodedConnect);
        transaction->persistConnection = true;
        if (connectionState->broken == false)
        {
            transaction->transactionState = Transaction::State::WaitingForResult;
        } else
        { transaction->transactionState = Transaction::State::NetworkError; }
        connectionState->currentTransaction = transaction;
        impl_->AddTransaction(transaction);
        return transaction;
    }

    auto MqttClient::Ping(const std::string& brokerId) -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        std::weak_ptr<TransactionImpl> transactionWeak(transaction);
        const auto timeKeeperCopy = impl_->timeKeeper;
        transaction->receiveTimeoutToken = impl_->scheduler->Schedule(
            [transactionWeak, timeKeeperCopy]
            {
                auto transaction = transactionWeak.lock();
                if (transaction != nullptr)
                {
                    (void)transaction->MarkComplete(Transaction::State::TimedOut,
                                                    timeKeeperCopy->GetCurrentTime());
                }
            },
            impl_->timeKeeper->GetCurrentTime() + impl_->requestTimeoutSeconds);
        const auto now = impl_->timeKeeper->GetCurrentTime();
        auto connectionState = impl_->persistentConnections->AttachTransaction(
            brokerId, transaction, now, *impl_->scheduler);
        if ((connectionState->connection != nullptr))
        {
            std::weak_ptr<Impl> implWeak(impl_);
            std::weak_ptr<ConnectionState> connectionStateWeak(connectionState);
            connectionState->setInactivityTimeout = [implWeak, brokerId, connectionStateWeak]()
            {
                auto impl = implWeak.lock();
                auto connectionState = connectionStateWeak.lock();
                if ((impl == nullptr) || (connectionState == nullptr))
                { return; }
                connectionState->inactivityCallbackToken = impl->scheduler->Schedule(
                    [implWeak, brokerId, connectionStateWeak]
                    {
                        auto impl = implWeak.lock();
                        auto connectionState = connectionStateWeak.lock();
                        if ((impl == nullptr) || (connectionState == nullptr))
                        { return; }
                        std::lock_guard<decltype(connectionState->mutex)> connectionLock(
                            connectionState->mutex);
                        if (connectionState->currentTransaction.lock() != nullptr)
                        { return; }
                        // connectionState->broken = true;
                        // impl->persistentConnections->DropConnection(brokerId, connectionState);
                    },
                    impl->timeKeeper->GetCurrentTime() + impl->inactivityInterval);
            };
        } else
        { impl_->persistentConnections->DropConnection(brokerId, connectionState); }

        auto packet = PacketsBuilder::buildPingPacket();

        std::vector<uint8_t> encodedPing;
        auto size = packet->computePacketSize(true);

        encodedPing.reserve(static_cast<size_t>(size));
        auto packetSize = packet->serialize(encodedPing.data());
        if (size != packetSize)
        { encodedPing.resize(packetSize); }
        transaction->connectionState = connectionState;
        transaction->impl_ = impl_;
        connectionState->connection->SendData(encodedPing);
        transaction->persistConnection = true;
        if (connectionState->broken == false)
        {
            transaction->transactionState = Transaction::State::WaitingForResult;
        } else
        { transaction->transactionState = Transaction::State::NetworkError; }
        connectionState->currentTransaction = transaction;
        impl_->AddTransaction(transaction);
        return transaction;
    }

    auto MqttClient::Authenticate(const ReasonCode& reasonCode, const DynamicStringView& authMethod,
                                  const DynamicBinaryDataView& authData, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        return transaction;
    }

    auto MqttClient::Subscribe(const std::string& brokerId, const char* topic,
                               const RetainHandling retainHandling, const bool withAutoFeedBack,
                               const QoSDelivery maxAcceptedQos, const bool retainAsPublished,
                               Properties* properties) -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        impl_->state = ClientState::Subscribing;
        std::weak_ptr<TransactionImpl> transactionWeak(transaction);
        const auto timeKeeperCopy = impl_->timeKeeper;
        transaction->receiveTimeoutToken = impl_->scheduler->Schedule(
            [transactionWeak, timeKeeperCopy]
            {
                auto transaction = transactionWeak.lock();
                if (transaction != nullptr)
                {
                    (void)transaction->MarkComplete(Transaction::State::TimedOut,
                                                    timeKeeperCopy->GetCurrentTime());
                }
            },
            impl_->timeKeeper->GetCurrentTime() + impl_->requestTimeoutSeconds);

        const auto now = impl_->timeKeeper->GetCurrentTime();
        auto connectionState = impl_->persistentConnections->AttachTransaction(
            brokerId, transaction, now, *impl_->scheduler);
        if ((connectionState->connection != nullptr))
        {
            std::weak_ptr<Impl> implWeak(impl_);
            std::weak_ptr<ConnectionState> connectionStateWeak(connectionState);
            connectionState->setInactivityTimeout = [implWeak, brokerId, connectionStateWeak]()
            {
                auto impl = implWeak.lock();
                auto connectionState = connectionStateWeak.lock();
                if ((impl == nullptr) || (connectionState == nullptr))
                { return; }
                connectionState->inactivityCallbackToken = impl->scheduler->Schedule(
                    [implWeak, brokerId, connectionStateWeak]
                    {
                        auto impl = implWeak.lock();
                        auto connectionState = connectionStateWeak.lock();
                        if ((impl == nullptr) || (connectionState == nullptr))
                        { return; }
                        std::lock_guard<decltype(connectionState->mutex)> connectionLock(
                            connectionState->mutex);
                        if (connectionState->currentTransaction.lock() != nullptr)
                        { return; }
                        // connectionState->broken = true;
                        // impl->persistentConnections->DropConnection(brokerId, connectionState);
                    },
                    impl->timeKeeper->GetCurrentTime() + impl->inactivityInterval);
            };
        } else
        { impl_->persistentConnections->DropConnection(brokerId, connectionState); }

        transaction->packetID = impl_->nextPacketId();
        auto packet = PacketsBuilder::buildSubscribePacket(
            transaction->packetID, topic, retainHandling, withAutoFeedBack, maxAcceptedQos,
            retainHandling, properties);
        auto size = packet->computePacketSize(true);
        std::vector<uint8_t> encodedConnect;
        encodedConnect.reserve((size_t)size);
        auto packetSize = packet->serialize(encodedConnect.data());
        if (size != packetSize)
        { encodedConnect.resize(packetSize); }
        transaction->connectionState = impl_->connectionState.lock();
        transaction->impl_ = impl_;
        impl_->connectionState.lock()->connection->SendData(encodedConnect);
        transaction->persistConnection = true;
        if (impl_->connectionState.lock()->broken == false)
        {
            transaction->transactionState = Transaction::State::WaitingForResult;
        } else
        { transaction->transactionState = Transaction::State::NetworkError; }
        impl_->connectionState.lock()->currentTransaction = transaction;
        return transaction;
    }

    auto MqttClient::Subscribe(const std::string& brokerId, SubscribeTopic* topics,
                               Properties* properties) -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        if (impl_->connectionState.lock()->broken)
        {
            impl_->diagnosticSender.SendDiagnosticInformationFormatted(
                0, "Connection: State %d", Transaction::State::NetworkError);
        }
        impl_->state = ClientState::Subscribing;
        std::weak_ptr<TransactionImpl> transactionWeak(transaction);
        const auto timeKeeperCopy = impl_->timeKeeper;
        transaction->receiveTimeoutToken = impl_->scheduler->Schedule(
            [transactionWeak, timeKeeperCopy]
            {
                auto transaction = transactionWeak.lock();
                if (transaction != nullptr)
                {
                    (void)transaction->MarkComplete(Transaction::State::TimedOut,
                                                    timeKeeperCopy->GetCurrentTime());
                }
            },
            impl_->timeKeeper->GetCurrentTime() + impl_->requestTimeoutSeconds);

        const auto now = impl_->timeKeeper->GetCurrentTime();
        auto connectionState = impl_->persistentConnections->AttachTransaction(
            brokerId, transaction, now, *impl_->scheduler);
        if ((connectionState->connection != nullptr))
        {
            std::weak_ptr<Impl> implWeak(impl_);
            std::weak_ptr<ConnectionState> connectionStateWeak(connectionState);
            connectionState->setInactivityTimeout = [implWeak, brokerId, connectionStateWeak]()
            {
                auto impl = implWeak.lock();
                auto connectionState = connectionStateWeak.lock();
                if ((impl == nullptr) || (connectionState == nullptr))
                { return; }
                connectionState->inactivityCallbackToken = impl->scheduler->Schedule(
                    [implWeak, brokerId, connectionStateWeak]
                    {
                        auto impl = implWeak.lock();
                        auto connectionState = connectionStateWeak.lock();
                        if ((impl == nullptr) || (connectionState == nullptr))
                        { return; }
                        std::lock_guard<decltype(connectionState->mutex)> connectionLock(
                            connectionState->mutex);
                        if (connectionState->currentTransaction.lock() != nullptr)
                        { return; }
                        // connectionState->broken = true;
                        // impl->persistentConnections->DropConnection(brokerId, connectionState);
                    },
                    impl->timeKeeper->GetCurrentTime() + impl->inactivityInterval);
            };
        } else
        { impl_->persistentConnections->DropConnection(brokerId, connectionState); }

        transaction->packetID = impl_->nextPacketId();
        auto packet =
            PacketsBuilder::buildSubscribePacket(transaction->packetID, topics, properties);
        auto size = packet->computePacketSize(true);
        std::vector<uint8_t> encodedConnect;
        encodedConnect.reserve((size_t)size);
        auto packetSize = packet->serialize(encodedConnect.data());
        if (size != packetSize)
        { encodedConnect.resize(packetSize); }
        transaction->connectionState = impl_->connectionState.lock();
        transaction->impl_ = impl_;
        impl_->connectionState.lock()->connection->SendData(encodedConnect);
        transaction->persistConnection = true;
        if (impl_->connectionState.lock()->broken == false)
        {
            transaction->transactionState = Transaction::State::WaitingForResult;
        } else
        { transaction->transactionState = Transaction::State::NetworkError; }
        impl_->connectionState.lock()->currentTransaction = transaction;
        return transaction;
    }

    auto MqttClient::Unsubscribe(UnsubscribeTopic* topics, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        if (impl_->connectionState.lock()->broken)
        {
            impl_->diagnosticSender.SendDiagnosticInformationFormatted(
                0, "Connection: State %d", Transaction::State::NetworkError);
            return transaction;
        }
        impl_->state = ClientState::Unsubscribing;
        transaction->packetID = impl_->nextPacketId();
        auto packet =
            PacketsBuilder::buildUnsubscribePacket(transaction->packetID, topics, properties);
        auto size = packet->computePacketSize(true);
        std::vector<uint8_t> encodedConnect;
        encodedConnect.reserve((size_t)size);
        auto packetSize = packet->serialize(encodedConnect.data());
        if (size != packetSize)
        { encodedConnect.resize(packetSize); }
        transaction->connectionState = impl_->connectionState.lock();
        transaction->impl_ = impl_;
        impl_->connectionState.lock()->connection->SendData(encodedConnect);
        transaction->persistConnection = true;
        if (impl_->connectionState.lock()->broken == false)
        {
            transaction->transactionState = Transaction::State::WaitingForResult;
        } else
        { transaction->transactionState = Transaction::State::NetworkError; }
        impl_->connectionState.lock()->currentTransaction = transaction;
        return transaction;
    }

    auto MqttClient::Publish(const std::string& brokerId, const std::string topic,
                             const std::string payload, const bool retain, const QoSDelivery qos,
                             const uint16_t packetID, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        if (impl_->connectionState.lock()->broken)
        {
            impl_->diagnosticSender.SendDiagnosticInformationFormatted(
                0, "Connection: State %d", Transaction::State::NetworkError);
            return transaction;
        }
        transaction->packetID = packetID;
        impl_->state = ClientState::Publishing;
        Utf8::Utf8 utf8;
        const auto utf8Payload = utf8.Encode(Utf8::AsciiToUnicode(payload));
        auto packet = PacketsBuilder::buildPublishPacket(transaction->packetID, topic, utf8Payload,
                                                         (uint32_t)utf8Payload.size(), qos, retain,
                                                         properties);
        auto size = packet->computePacketSize();
        std::vector<uint8_t> encodedConnect;
        encodedConnect.reserve(static_cast<size_t>(size));
        auto packetSize = packet->serialize(encodedConnect.data());
        if (packetSize != size)
        { encodedConnect.resize(packetSize); }
        if (qos == QoSDelivery::AtLeastOne)
        {
            impl_->buffers.storeQos1ID(transaction->packetID);
            impl_->saveQoS(transaction->packetID, encodedConnect);
        } else if (qos == QoSDelivery::ExactlyOne)
        {
            impl_->buffers.storeQoS2ID(transaction->packetID);
            impl_->saveQoS(transaction->packetID, encodedConnect);
        }
        transaction->connectionState = impl_->connectionState.lock();
        transaction->impl_ = impl_;
        impl_->connectionState.lock()->connection->SendData(encodedConnect);
        transaction->persistConnection = true;
        if (impl_->connectionState.lock()->broken == false)
        {
            if (qos == QoSDelivery::AtMostOne)
            {
                transaction->transactionState = Transaction::State::Success;
            } else
            { transaction->transactionState = Transaction::State::WaitingForResult; }

        } else
        { transaction->transactionState = Transaction::State::NetworkError; }
        impl_->connectionState.lock()->currentTransaction = transaction;
        return transaction;
    }

    MqttClient::MqttClient(const char* clientID, Storage::MessageReceived* callback,
                           const Storage::DynamicBinaryDataView* brokerCert,
                           Storage::PacketStore* storage) :
        impl_(std::make_shared<Impl>(clientID, callback, brokerCert, storage)) {}
    MqttClient::~MqttClient() {}

    MqttClient::MqttClient(const MqttClient& o) : impl_(o.impl_) {}

    MqttClient::MqttClient(MqttClient&& o) : impl_(std::move(o.impl_)) {}

    MqttClient& MqttClient::operator=(const MqttClient& o) {
        if (this != &o)
            impl_ = o.impl_;
        return *this;
    }

    MqttClient& MqttClient::operator=(MqttClient&& o) {
        if (this != &o)
            impl_ = std::move(o.impl_);
        return *this;
    }

    void PrintTo(const MqttV5::MqttClient::ClientState& state, std::ostream* os) {
        switch (state)
        {
        case MqttV5::MqttClient::ClientState::Connecting: {
            *os << "Connecting";
        }
        break;
        case MqttV5::MqttClient::ClientState::Publishing: {
            *os << "Publishing";
        }
        break;
        case MqttV5::MqttClient::ClientState::Subscribing: {
            *os << "Subscribing";
        }
        break;
        case MqttV5::MqttClient::ClientState::Unsubscribing: {
            *os << "Unsubscribing";
        }
        break;
        case MqttV5::MqttClient::ClientState::Pinging: {
            *os << "Pinging";
        }
        break;
        case MqttV5::MqttClient::ClientState::Authenticating: {
            *os << "Authenticating";
        }
        break;
        case MqttV5::MqttClient::ClientState::Disconnecting: {
            *os << "Disconnecting";
        }
        break;
        default: {
            *os << "???";
        };
        }
    }
}  // namespace MqttV5
