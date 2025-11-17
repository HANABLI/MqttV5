/**
 * @file MqttClient.cpp
 * @brief This file contains the implementation of the MqttClient class.
 * @note This class is used to connect to an MQTT broker and send/receive messages.
 * @note This class is used to store packets in a circular buffer.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */

#include "MqttV5/MqttClient.hpp"
#include <vcruntime.h>
#include <string>
#include <memory>
#include <mutex>

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
         * Last inactivity for inactivity managment.
         */
        double lastActivitySeconds = 0.0;
    };

    /**
     * This is a client transaction structure that contains the additional
     * properties and methods required by the client's implementation.
     */
    struct TransactionImpl : public MqttClient::Transaction
    {
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
        std::function<void(Transaction::State state)> completionDelegate;

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

        using State = MqttClient::Transaction::State;
        /**
         * This is the transaction state
         */
        State transactionState;

        void AwaitCompletion() override {}
        void SetCompletionDelegate(std::function<void(Transaction::State state)> cb) override;

        void MarkComplete(State s);

        // déclarations seulement - supprimer les bodies inline
        void HandleConnAck(const uint8_t* packetPtr, uint32_t packetSize);
        void DataReceived(const std::vector<uint8_t>& data, double now);
        void ConnectionBroken(double now);
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
         * This is the delegate to generate diagnostic messages
         * for the client.
         */
        SystemUtils::DiagnosticsSender diagnosticSender;
        // This is used to track if the client is mobilized
        bool mobilized = false;
        // This is the transport layer implementation to use.
        std::shared_ptr<ClientTransportLayer> transport;

        // This is the object used to track time in the client.
        std::shared_ptr<TimeKeeper> timeKeeper;

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
                              const std::string& scheme, const std::string& brokerHost,
                              uint16_t port) -> std::shared_ptr<ConnectionState> {
            // TODO either get a connection from a pool of connection or a map of connection neither
            // create a new one.

            const auto connectionState = std::make_shared<ConnectionState>();
            std::weak_ptr<ConnectionState> connectionStateWeak(connectionState);
            auto timeKeeperRef = timeKeeper;

            connectionState->connection = transport->Connect(
                scheme, brokerHost, port,
                [connectionStateWeak, timeKeeperRef](const std::vector<uint8_t>& data)
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
                    transaction->DataReceived(data, timeKeeperRef->GetCurrentTime());
                },
                [connectionStateWeak, timeKeeperRef](bool)
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
                });
            connectionState->currentTransaction = transaction;
            return connectionState;
        }
    };

    /**
     *
     *
     */

    void TransactionImpl::SetCompletionDelegate(std::function<void(Transaction::State state)> cb) {
        std::unique_lock<decltype(mutex)> lock(mutex);
        this->completionDelegate = cb;
        const bool wasComplete = complete;
        lock.unlock();
        if (wasComplete)
        { completionDelegate(transactionState); }
    }

    void TransactionImpl::MarkComplete(State s) {
        if (complete)
        { return; }
        bool dropConnection = false;
        std::function<void(Transaction::State state)> cb;
        {
            std::lock_guard<decltype(mutex)> lock(mutex);
            if (complete)
                return;
            transactionState = s;
            complete = true;
            cb = completionDelegate;
        }
        if (cb)
        { cb(s); }
    }

    void TransactionImpl::HandleConnAck(const uint8_t* packetPtr, uint32_t packetSize) {
        MqttV5::ConnAckPacket receivedAck;
        uint32_t desSize = receivedAck.deserialize(packetPtr, packetSize);
        if (!receivedAck.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket);
            return;
        }
        // if (!receivedAck.props.checkPropertiesFor(ControlPacketType::CONNACK))
        // {
        //     MarkComplete(State::BadProperties);
        //     return;
        // }
        auto impl = impl_.lock();
        auto rc = receivedAck.fixedVariableHeader.reasonCode;
        if (rc == Storage::ReasonCode::Success)
        {
            if (connectionState && impl)
            {
                impl->retransmitPending(connectionState);
                MarkComplete(State::Success);
            }
        } else
        {
            MarkComplete(State::NetworkError);
            if (impl && impl->cb)
            {
                auto r = static_cast<Storage::ReasonCode>(rc);
                impl->cb->onConnectionLost(r);
            }
        }
    }

    void TransactionImpl::DataReceived(const std::vector<uint8_t>& data, double now) {
        std::unique_lock<decltype(mutex)> lock(mutex);
        auto connectionStateRef = connectionState;
        if (complete)
        { return; }
        auto& rxBuf = connectionStateRef->rxBuffer;
        rxBuf.insert(rxBuf.end(), data.begin(), data.end());

        size_t offset = 0;

        while (true)
        {
            size_t available = rxBuf.size() - offset;
            if (available < 2)
                break;
            ControlPacketType type;
            FixedHeader header;
            header.raw = rxBuf[offset];
            type = (ControlPacketType)(uint8_t)header.type;
            Mqtt_V5::VBInt len;
            uint32_t r = len.readFrom(rxBuf.data() + 4, (uint32_t)available - 4);
            if (r == MqttV5::Mqtt_V5::BadData || r == Mqtt_V5::NotEnoughData)
                break;
            uint32_t remainingLength = len;
            uint32_t totalPacketSize = remainingLength + len.getSerializedSize() + 4;
            if (available < totalPacketSize)
            { break; }

            const uint8_t* packetPtr = rxBuf.data() + offset;
            uint32_t packetSize = totalPacketSize;

            switch (type)
            {
            case MqttV5::ControlPacketType::CONNACK:
                HandleConnAck(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBLISH:
                // HandlePublish(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBACK:
                // HandlePubAck(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::SUBSCRIBE:
                // HandleSubscribe(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::SUBACK:
                // HandleSubAck(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::UNSUBSCRIBE:
                // HandleUnsubscribe(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PINGRESP:
                // HandlePingResp(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PINGREQ:
                // HandlePingReq(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBREL:
                // HandlePubRel(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBREC:
                // HandlePubRec(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBCOMP:
                // HandlePubComp(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::AUTH:
                // HandleAuth(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::DISCONNECT:
                // HandleDisconnect(packetPtr, packetSize);
                break;
            default:
                break;
            }

            offset += totalPacketSize;
        }
        if (offset > 0)
            rxBuf.erase(rxBuf.begin(), rxBuf.begin() + offset);
        lock.unlock();
    }

    void TransactionImpl::ConnectionBroken(double now) {
        std::unique_lock<decltype(mutex)> lock(mutex);
        if (complete)
        { return; }

        lock.unlock();
        MarkComplete(State::NetworkError);
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
        impl_->mobilized = true;
    }

    void MqttClient::Demobilize() {
        impl_->timeKeeper = nullptr;
        impl_->transport = nullptr;
        impl_->mobilized = false;
    }

    SystemUtils::DiagnosticsSender::UnsubscribeDelegate MqttClient::SubscribeToDiagnostics(
        SystemUtils::DiagnosticsSender::DiagnosticMessageDelegate delegate, size_t minLevel) {
        return impl_->diagnosticSender.SubscribeToDiagnostics(delegate, minLevel);
    }

    auto MqttClient::ConnectTo(const std::string& brokerHost, const uint16_t port, bool useTLS,
                               const bool cleanSession, const uint16_t keepAlive,
                               const char* userName, const DynamicBinaryData* password,
                               WillMessage* willMessage, const QoSDelivery willQoS,
                               const bool willRetain, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        if (!impl_->transport || !impl_->mobilized)
        {
            impl_->diagnosticSender.SendDiagnosticInformationString(
                0, "MqttClient::ConnectTo called before Server Mobilization");
            return nullptr;
        }
        const auto transaction = std::make_shared<TransactionImpl>();

        const std::string scheme = useTLS ? "mqtts" : "mqtt";
        auto connectionState = impl_->CreateConnection(transaction, scheme, brokerHost, port);
        if (connectionState->broken)
        {
            impl_->diagnosticSender.SendDiagnosticInformationFormatted(
                0, "Connection: State %d", Transaction::State::NetworkError);
        }
        impl_->keepAlive =
            (keepAlive + (keepAlive / 2)) /
            2;  // Make it 75% of what's given so we always wake up before doom's clock
        auto packet = PacketsBuilder::buildConnectPacket(impl_->clientID, userName, password, true,
                                                         impl_->keepAlive, willMessage, willQoS,
                                                         willRetain, properties);
        // TODO check validation in the PacketsBuilder module
        // if (impl_->options.avoidValidation)
        // {
        //     if (!packet->getProperties().checkPropertiesFor(CONNECT))
        //         impl_->diagnosticSender.SendDiagnosticInformationFormatted(
        //             0, "Check properties for Connection: State %d",
        //             Transaction::State::BadProperties);
        // }
        uint8_t encodedConnect[1024] = {};
        auto packetSize = packet->serialize(encodedConnect);
        std::vector<uint8_t> data(encodedConnect, encodedConnect + packetSize);
        transaction->connectionState = connectionState;
        transaction->impl_ = impl_;
        connectionState->connection->SendData(data);

        transaction->persistConnection = true;
        transaction->transactionState = Transaction::State::WaitingForResult;
        connectionState->currentTransaction = transaction;
        return transaction;
    }

    auto MqttClient::Authenticate(const ReasonCode& reasonCode, const DynamicStringView& authMethod,
                                  const DynamicBinaryDataView& authData, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        return transaction;
    }

    auto MqttClient::Subscribe(const char* topic, const RetainHandling retainHandling,
                               const bool withAutoFeedBack, const QoSDelivery maxAcceptedQos,
                               const bool retainAsPublished, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        return transaction;
    }

    auto MqttClient::Subscribe(SubscribeTopic& topics, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        return transaction;
    }

    auto MqttClient::Unsubscribe(SubscribeTopic& topics, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        return transaction;
    }

    auto MqttClient::Publish(const char* topic, const char* payload, const bool retain,
                             const QoSDelivery QoS, const uint16_t packetID, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
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
}  // namespace MqttV5
