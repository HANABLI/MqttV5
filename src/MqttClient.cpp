/**
 * @file MqttClient.cpp
 * @brief This file contains the implementation of the MqttClient class.
 * @note This class is used to connect to an MQTT broker and send/receive messages.
 * @note This class is used to store packets in a circular buffer.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */

#include "MqttV5/MqttClient.hpp"
#include <Utf8/Utf8.hpp>
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
         * This is a vector of reason received on response to a transaction.
         */
        std::vector<Storage::ReasonCode> reasons;

        using State = MqttClient::Transaction::State;

        void AwaitCompletion() override {}

        void SetCompletionDelegate(
            std::function<void(std::vector<ReasonCode>& reasons)> cb) override;

        void MarkComplete(State reasons);

        // déclarations seulement - supprimer les bodies inline
        void HandleConnAck(const uint8_t* packetPtr, uint32_t packetSize);
        void HandleSubAck(const uint8_t* packetPtr, uint32_t packetSize);
        void HandleUnSubAck(const uint8_t* packetPtr, uint32_t packetSize);
        void HandlePublish(const uint8_t* packetPtr, uint32_t packetSize);
        void HandlePubAck(const uint8_t* packetPtr, uint32_t packetSize);
        void HandlePubRec(const uint8_t* packetPtr, uint32_t packetSize);
        void HandlePubComp(const uint8_t* packetPtr, uint32_t packetSize);
        void HandlePubRel(const uint8_t* packetPtr, uint32_t packetSize);
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

        // This is the connection state
        std::weak_ptr<ConnectionState> connectionState;

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

    void TransactionImpl::SetCompletionDelegate(std::function<void(std::vector<ReasonCode>&)> cb) {
        std::unique_lock<decltype(mutex)> lock(mutex);
        this->completionDelegate = cb;
        const bool wasComplete = complete;
        lock.unlock();
        if (wasComplete)
        { cb(reasons); }
    }

    void TransactionImpl::MarkComplete(State state) {
        if (complete)
        { return; }
        bool dropConnection = connectionState->broken;
        std::function<void(std::vector<ReasonCode>&)> cb;
        {
            std::lock_guard<decltype(mutex)> lock(mutex);
            if (complete)
                return;
            transactionState = state;
            complete = true;
            cb = completionDelegate;
        }
        if (cb)
        {
            cb(reasons);
            reasons.erase(reasons.begin(), reasons.end());
        }
    }

    void TransactionImpl::HandleUnSubAck(const uint8_t* packetPtr, uint32_t packetSize) {
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

        MarkComplete(allSuccess);
    }

    void TransactionImpl::HandleSubAck(const uint8_t* packetPtr, uint32_t packetSize) {
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

        MarkComplete(allSuccess);
    }

    void TransactionImpl::HandleConnAck(const uint8_t* packetPtr, uint32_t packetSize) {
        ConnAckPacket receivedAck;
        uint32_t desSize = receivedAck.deserialize(packetPtr, packetSize);
        if (!receivedAck.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedAck.props.checkPropertiesFor(ControlPacketType::CONNACK))
            {
                MarkComplete(State::BadProperties);
                return;
            }
        }
        reasons.push_back((ReasonCode)receivedAck.fixedVariableHeader.reasonCode);

        if (reasons.back() == Storage::ReasonCode::Success)
        {
            if (connectionState && impl)
            {
                impl->retransmitPending(connectionState);
                MarkComplete(State::Success);
            }
        } else
        {
            MarkComplete(State::NetworkError);
            if (impl && impl->cb && connectionState->broken)
            { (void)impl->cb->onConnectionLost(State::NetworkError); }
        }
    }

    void TransactionImpl::HandlePublish(const uint8_t* packetPtr, uint32_t packetSize) {
        PublishPacket pubPacket;
        auto desSize = pubPacket.deserialize(packetPtr, packetSize);
        if (!pubPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!pubPacket.props.checkPropertiesFor(ControlPacketType::PUBLISH))
            {
                MarkComplete(State::BadProperties);
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
            auto pubAckBuff = new uint8_t[(size_t)pubAckPacketSize];
            auto pubAckBuffSize = pubAckPacket->serialize(pubAckBuff);
            std::vector<uint8_t> data(pubAckBuff, pubAckBuff + pubAckBuffSize);
            connectionState->connection->SendData(data);
            MarkComplete(State::Success);
            return;
        } else if (qos == QoSDelivery::ExactlyOne)
        {
            auto pubRecPacket =
                PacketsBuilder::buildPubRecPacket(packetID, ReasonCode::Success, nullptr);
            auto pubRecPacketSize = pubRecPacket->computePacketSize(true);
            auto pubRecBuff = new uint8_t[(size_t)pubRecPacketSize];
            auto pubRecBuffSize = pubRecPacket->serialize(pubRecBuff);
            std::vector<uint8_t> data(pubRecBuff, pubRecBuff + pubRecBuffSize);
            connectionState->connection->SendData(data);
            return;
        }
        MarkComplete(State::Success);
    }

    void TransactionImpl::HandlePubAck(const uint8_t* packetPtr, uint32_t packetSize) {
        PubAckPacket receivedPacket;
        auto desSize = receivedPacket.deserialize(packetPtr, packetSize);
        if (!receivedPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedPacket.props.checkPropertiesFor(ControlPacketType::PUBACK))
            { MarkComplete(State::BadProperties); }
        }
        auto id = receivedPacket.fixedVariableHeader.packetID;

        auto i = impl->buffers.findID(id);
        if (i < impl->buffers.end())
        {
            impl->buffers.releaseID(id);
            (void)impl->removeQos(id);
        } else
        {
            MarkComplete(State::StorageError);
            return;
        }
        reasons.push_back((ReasonCode)receivedPacket.fixedVariableHeader.reasonCode);
        if (reasons.back() == Storage::ReasonCode::Success)
        {
            MarkComplete(State::Success);
        } else
        {
            MarkComplete(State::NetworkError);
            if (impl && impl->cb && connectionState->broken)
            { (void)impl->cb->onConnectionLost(State::NetworkError); }
        }
    }

    void TransactionImpl::HandlePubRec(const uint8_t* packetPtr, uint32_t packetSize) {
        PubAckPacket receivedPacket;
        auto desSize = receivedPacket.deserialize(packetPtr, packetSize);
        if (!receivedPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedPacket.props.checkPropertiesFor(ControlPacketType::PUBREC))
            {
                MarkComplete(State::BadProperties);
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
        auto encodedConnect = new uint8_t[(size_t)size];
        auto serSize = packet->serialize(encodedConnect);
        std::vector<uint8_t> data(encodedConnect, encodedConnect + serSize);
        impl->connectionState.lock()->connection->SendData(data);
    }

    void TransactionImpl::HandlePubComp(const uint8_t* packetPtr, uint32_t packetSize) {
        PubCompPacket receivedPacket;
        auto desSize = receivedPacket.deserialize(packetPtr, packetSize);
        if (!receivedPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedPacket.props.checkPropertiesFor(ControlPacketType::PUBCOMP))
            {
                MarkComplete(State::BadProperties);
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

        MarkComplete(allSuccess);
    }

    void TransactionImpl::HandlePubRel(const uint8_t* packetPtr, uint32_t packetSize) {
        PubRelPacket receivedPacket;
        auto desSize = receivedPacket.deserialize(packetPtr, packetSize);
        if (!receivedPacket.checkImpl() || (desSize != packetSize))
        {
            MarkComplete(State::ShunkedPacket);
            return;
        }
        auto impl = impl_.lock();
        if (impl->options.avoidValidation)
        {
            if (!receivedPacket.props.checkPropertiesFor(ControlPacketType::PUBREL))
            {
                MarkComplete(State::BadProperties);
                return;
            }
        }

        auto pubCompPacket =
            PacketsBuilder::buildPubCompPacket(packetID, ReasonCode::Success, nullptr);
        auto pubCompPacketSize = pubCompPacket->computePacketSize(true);
        auto pubCompBuff = new uint8_t[(size_t)pubCompPacketSize];
        auto pubCompBuffSize = pubCompPacket->serialize(pubCompBuff);
        std::vector<uint8_t> data(pubCompBuff, pubCompBuff + pubCompBuffSize);
        connectionState->connection->SendData(data);
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
                HandleConnAck(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBLISH:
                HandlePublish(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBACK:
                HandlePubAck(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::SUBSCRIBE:
                // HandleSubscribe(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::SUBACK:
                HandleSubAck(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::UNSUBACK:
                HandleUnSubAck(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PINGRESP:
                // HandlePingResp(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PINGREQ:
                // HandlePingReq(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBREL:
                HandlePubRel(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBREC:
                HandlePubRec(packetPtr, packetSize);
                break;
            case MqttV5::ControlPacketType::PUBCOMP:
                HandlePubComp(packetPtr, packetSize);
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
        impl_->state = MqttClient::ClientState::Connecting;
        auto connectionState = impl_->CreateConnection(transaction, scheme, brokerHost, port);
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

        auto size = packet->computePacketSize(true);
        uint8_t* encodedConnect = new uint8_t[(size_t)size];
        auto packetSize = packet->serialize(encodedConnect);
        std::vector<uint8_t> data(encodedConnect, encodedConnect + packetSize);
        delete[] encodedConnect;
        transaction->connectionState = connectionState;
        transaction->impl_ = impl_;
        connectionState->connection->SendData(data);

        transaction->persistConnection = true;
        if (connectionState->broken == false)
        {
            transaction->transactionState = Transaction::State::WaitingForResult;
        } else
        { transaction->transactionState = Transaction::State::NetworkError; }
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
        if (impl_->connectionState.lock()->broken)
        {
            impl_->diagnosticSender.SendDiagnosticInformationFormatted(
                0, "Connection: State %d", Transaction::State::NetworkError);
        }
        impl_->state = ClientState::Subscribing;
        transaction->packetID = impl_->nextPacketId();
        auto packet = PacketsBuilder::buildSubscribePacket(
            transaction->packetID, topic, retainHandling, withAutoFeedBack, maxAcceptedQos,
            retainHandling, properties);
        auto size = packet->computePacketSize(true);
        uint8_t* encodedConnect = new uint8_t[(size_t)size];
        auto packetSize = packet->serialize(encodedConnect);
        std::vector<uint8_t> data(encodedConnect, encodedConnect + packetSize);
        delete[] encodedConnect;
        transaction->connectionState = impl_->connectionState.lock();
        transaction->impl_ = impl_;
        impl_->connectionState.lock()->connection->SendData(data);
        transaction->persistConnection = true;
        if (impl_->connectionState.lock()->broken == false)
        {
            transaction->transactionState = Transaction::State::WaitingForResult;
        } else
        { transaction->transactionState = Transaction::State::NetworkError; }
        impl_->connectionState.lock()->currentTransaction = transaction;
        return transaction;
    }

    auto MqttClient::Subscribe(SubscribeTopic* topics, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        if (impl_->connectionState.lock()->broken)
        {
            impl_->diagnosticSender.SendDiagnosticInformationFormatted(
                0, "Connection: State %d", Transaction::State::NetworkError);
        }
        impl_->state = ClientState::Subscribing;
        transaction->packetID = impl_->nextPacketId();
        auto packet =
            PacketsBuilder::buildSubscribePacket(transaction->packetID, topics, properties);
        auto size = packet->computePacketSize(true);
        uint8_t* encodedConnect = new uint8_t[(size_t)size];
        auto packetSize = packet->serialize(encodedConnect);
        std::vector<uint8_t> data(encodedConnect, encodedConnect + packetSize);
        delete[] encodedConnect;
        transaction->connectionState = impl_->connectionState.lock();
        transaction->impl_ = impl_;
        impl_->connectionState.lock()->connection->SendData(data);
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
        uint8_t* encodedConnect = new uint8_t[(size_t)size];
        auto packetSize = packet->serialize(encodedConnect);
        std::vector<uint8_t> data(encodedConnect, encodedConnect + packetSize);
        delete[] encodedConnect;
        transaction->connectionState = impl_->connectionState.lock();
        transaction->impl_ = impl_;
        impl_->connectionState.lock()->connection->SendData(data);
        transaction->persistConnection = true;
        if (impl_->connectionState.lock()->broken == false)
        {
            transaction->transactionState = Transaction::State::WaitingForResult;
        } else
        { transaction->transactionState = Transaction::State::NetworkError; }
        impl_->connectionState.lock()->currentTransaction = transaction;
        return transaction;
    }

    auto MqttClient::Publish(const std::string topic, const std::string payload, const bool retain,
                             const QoSDelivery qos, const uint16_t packetID, Properties* properties)
        -> std::shared_ptr<MqttClient::Transaction> {
        const auto transaction = std::make_shared<TransactionImpl>();
        if (impl_->connectionState.lock()->broken)
        {
            impl_->diagnosticSender.SendDiagnosticInformationFormatted(
                0, "Connection: State %d", Transaction::State::NetworkError);
            return transaction;
        }
        transaction->packetID = impl_->nextPacketId();
        impl_->state = ClientState::Publishing;
        Utf8::Utf8 utf8;
        const auto utf8Payload = utf8.Encode(Utf8::AsciiToUnicode(payload));
        auto packet = PacketsBuilder::buildPublishPacket(transaction->packetID, topic, utf8Payload,
                                                         (uint32_t)utf8Payload.size(), qos, retain,
                                                         properties);
        auto size = packet->computePacketSize();
        uint8_t* encodedConnect = new uint8_t[(size_t)size];
        auto packetSize = packet->serialize(encodedConnect);
        std::vector<uint8_t> data(encodedConnect, encodedConnect + packetSize);
        if (qos == QoSDelivery::AtLeastOne)
        {
            impl_->buffers.storeQos1ID(transaction->packetID);
            impl_->saveQoS(transaction->packetID, data);
        } else if (qos == QoSDelivery::ExactlyOne)
        {
            impl_->buffers.storeQoS2ID(transaction->packetID);
            impl_->saveQoS(transaction->packetID, data);
        }
        delete[] encodedConnect;
        transaction->connectionState = impl_->connectionState.lock();
        transaction->impl_ = impl_;
        impl_->connectionState.lock()->connection->SendData(data);
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
