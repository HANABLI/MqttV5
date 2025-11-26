#ifndef MQTT_MQTTCLIENT_HPP
#define MQTT_MQTTCLIENT_HPP
/**
 * @file MqttClient.hpp
 * @brief this file contains the definition of the MqttClient class.
 * @note This class is used to connect to an MQTT broker and send/receive messages.
 * @note This class is used to store packets in a circular buffer.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 *
 */
#pragma once
#include <memory>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <functional>
#include "SystemUtils/DiagnosticsSender.hpp"
#include "IMqttClient.hpp"
#include "Connection.hpp"
#include "MqttV5Packets.hpp"
#include "ClientTransportLayer.hpp"
#include "MqttV5Constants.hpp"
#include "TimeKeeper.hpp"
#include "MqttV5Types.hpp"
#include "MqttV5Properties.hpp"

namespace
{
    template <typename T>
    inline T max(T a, T b) {
        return a > b ? a : b;
    }
    template <typename T>
    inline T min(T a, T b) {
        return a < b ? a : b;
    }
    constexpr double DEFAULT_REQUEST_TIMEOUT_SECONDS = 30.0;  // Default request timeout in seconds
    constexpr double DEFAULT_INACTIVITY_INTERVAL_SECONDS =
        60.0;  // Default inactivity interval in seconds

    struct Buffer
    {
        uint8_t* recvBuffer() { return end() * sizeof(uint32_t) + buffer; }
        const uint8_t* recvBuffer() const { return end() * sizeof(uint32_t) + buffer; }
        uint8_t findID(uint32_t ID) {
            for (uint8_t i = 0; i < end(); i++)
            {
                if ((packetsID()[i] & 0x1FFFF) == ID)
                { return i; }
            }
            return maxID;
        }
        bool clearSetID(uint32_t set, uint32_t clear = 0) {
            uint8_t i = findID(clear);
            if (i == end())
                return false;
            packetsID()[i] = set;
            return true;
        }
        static inline bool isSending(uint32_t ID) { return (ID & 0x10000) == 0 && ID; }
        static inline bool isQoS1(uint32_t ID) { return (ID & 0x80000000) == 0; }
        static inline bool isQoS2Step2(uint32_t ID) { return (ID & 0x40000000) != 0; }
        inline bool storeQos1ID(uint32_t ID) { return clearSetID((uint32_t)ID, 0); }
        inline bool storeQoS2ID(uint32_t ID) { return clearSetID((uint32_t)ID | 0x80000000, 0); }
        inline bool avanceQoS2(uint32_t ID) {
            uint8_t p = findID(ID);
            if (p == maxID)
                return false;
            packetsID()[p] |= 0x40000000;
            return true;
        }
        inline bool releaseID(uint32_t ID) { return clearSetID(0, (uint32_t)ID); }
        inline uint8_t end() const { return maxID; }
        inline uint8_t packetsCount() const { return maxID / 3; }
        inline void reset() { memset(packetsID(), 0, maxID * 3 * sizeof(uint32_t)); }
        inline uint32_t packetID(uint8_t i) const { return packetsID()[i]; }
        inline uint8_t countSentID() const {
            uint8_t count = 0;
            for (uint8_t i = 0; i < end(); i++)
            {
                if ((packetsID()[i] & 0x10000) == 0 && packetsID()[i])
                { count++; }
            }
            return count;
        }
        Buffer(uint32_t size, uint32_t maxID) :
            size(size),
            buffer((uint8_t*)::calloc(size + maxID * 3 * sizeof(uint32_t), 1)),
            maxID((uint8_t)(maxID * 3)) {}
        ~Buffer() {
            ::free(buffer);
            buffer = 0;
            size = 0;
            maxID = 0;
        }

    public:
        uint32_t size;

    private:
        uint32_t* packetsID() { return (uint32_t*)buffer; }
        const uint32_t* packetsID() const { return (uint32_t*)buffer; }
        uint8_t* buffer;
        uint8_t maxID;
    };

    struct PacketBookmark
    {
        uint16_t id;
        uint32_t size;
        uint32_t pos;

        inline void Set(uint16_t id, uint32_t size, uint32_t pos) {
            this->id = id;
            this->size = size;
            this->pos = pos;
        }
        PacketBookmark(uint16_t id = 0, uint32_t size = 0, uint32_t pos = 0) :
            id(id), size(size), pos(pos) {}
    };

}  // namespace
namespace MqttV5::Storage
{
    typedef MqttV5::Common::LocalError LocalError;
    typedef MqttV5::Mqtt_V5::DynamicBinaryData DynamicBinaryData;
    typedef MqttV5::Mqtt_V5::DynamicBinaryDataView DynamicBinaryDataView;
    typedef MqttV5::Mqtt_V5::DynamicString DynamicString;
    typedef MqttV5::Mqtt_V5::DynamicStringPair DynamicStringPair;
    typedef MqttV5::Mqtt_V5::DynamicStringView DynamicStringView;
    typedef MqttV5::ReasonCode ReasonCode;
    typedef MqttV5::Properties Properties;

    class PacketStore
    {
    public:
        virtual bool StorePacket(uint32_t packetID, uint32_t packetSize, const uint8_t* packet) = 0;
        // virtual bool RetrievePacket(uint32_t packetID, uint32_t& packetSize, uint8_t*& packet) =
        // 0;
        virtual bool RetrievePacket(uint32_t packetID, uint32_t& headSize, uint8_t*& headPacke,
                                    uint32_t& tailSize, uint8_t*& tailPacket) = 0;
        virtual bool RemovePacket(const uint32_t packetID) = 0;

    public:
        virtual ~PacketStore() = default;
    };
    /**
     * @brief This class implements a circular buffer to store packets.
     * @note This class is used to store packets in a circular buffer.
     */
    class CircularBufferStore : public PacketStore
    {
    public:
        // Methods
        bool StorePacket(uint32_t packetID, uint32_t packetSize, const uint8_t* packet) override;
        // bool RetrievePacket(uint32_t packetID, uint32_t& packetSize, uint8_t*& packet) override;
        bool RetrievePacket(uint32_t packetID, uint32_t& headSize, uint8_t*& headPacke,
                            uint32_t& tailSize, uint8_t*& tailPacket) override;
        bool RemovePacket(const uint32_t packetID) override;

        // Constructor
    public:
        CircularBufferStore(const uint32_t size, const uint32_t maxPacketsCount);
        ~CircularBufferStore() override;

    private:
        struct Impl;
        std::unique_ptr<struct Impl> impl_;
    };

    class MessageReceived
    {
    public:
        virtual void onMessageReceived(const DynamicStringView topic, DynamicBinaryDataView payload,
                                       uint16_t packetId) = 0;
        virtual uint32_t maxPacketSize() const { return 2048U; }   // Default max packet size;
        virtual uint32_t maxUnAckedPackets() const { return 1U; }  // Default max unacked packets;
        virtual bool onConnectionLost(const IMqttV5Client::Transaction::State& state) = 0;
        virtual ~MessageReceived() = default;
    };
}  // namespace MqttV5::Storage

namespace MqttV5
{
    using namespace Storage;

    class MqttClient : public IMqttV5Client
    {
    public:
        struct MqttOptions
        {
            /**
             * Set to true to use authentication with the broker.
             * Default: false
             */
            bool useAuthentication = false;
            /**
             * Set to true to reject unauthorized connections.
             * This is used to reject connections to brokers that do not have a valid certificate.
             * Default: false
             */
            bool rejectUnauthorized = false;
            /** Remove all validation from MQTT types.
             * This removes validation check for all MQTT types in order to save binary size.
             * This is only recommanded if you are sure about your broker implementation (don't set
             * this to 1 if you intend to connect to unknown broker) Default: 0
             * */
            bool avoidValidation = false;
            /**
             * Set to:
             * - 1 for enable complete QoS management. This imply allocating and using a buffer
             * that's 3x the maximum packet size you've set (for saving QoS packets against
             * disconnection) plus a small overhead in binary size.
             * - 0 to implement a non compliant, but QoS capable client. No packet are saved for
             * QoS, but the client will claim and follow QoS protocols for publish cycle. The binary
             * overhead is also reduced.
             * - -1 to disable QoS management. The client will never claim it is QoS capable. Saves
             * the maximum binary size. Default: 1
             */
            int qosSupportLevel = 1;
            /**
             * Set to true to use SSL for the connection.
             * Default: false
             */
            bool useSSL = false;
            /**
             * Set to true to use TLS for the connection.
             * Default: false
             */
            bool useTLS = false;
            /**
             * The period in milliseconds to wait before attempting to reconnect to the broker.
             * Default: 1000 milliseconds
             */
            uint32_t reconnectPeriod = 1000;  // milliseconds
            /**
             * The timeout in milliseconds to wait for a connection to the broker.
             * Default: 30000 milliseconds
             */
            uint32_t connectTimeout = 30000;  // milliseconds
            /**
             * Set to true to resubscribe to topics after reconnecting to the broker.
             * Default: true
             */
            bool resubscribe = true;  // Resubscribe to topics after reconnect
        };
        /**
         * This structure holds all of the configuration items
         * and dependency objects needed by the client when it's
         * mobilized.
         */
        struct MqttMobilizationDependencies
        {
            /**
             * This is the transport layer implementation to use.
             */
            std::shared_ptr<ClientTransportLayer> transport;

            /**
             * This is the object used to track time in the client.
             */
            std::shared_ptr<TimeKeeper> timeKeeper;

            /**
             * This is the amount of time after a request is made
             * of a server, before the transaction is considered timed out
             * if no part of a response has been received.
             */
            double requestTimeoutSeconds = 0.0;

            /**
             * This is the amount of time, after a transaction is completed,
             * that a persistent connection is closed if another transaction
             * does not reuse the connection.
             */
            double inactivityInterval = 0.0;

            /**
             * This
             */
            MqttOptions options;
        };

        enum ClientState : int
        {
            Unknown = 0,
            Connecting = 1,
            Authenticating = 2,
            Running = 3,
            Subscribing = 4,
            Publishing = 5,
            Unsubscribing = 6,
            Pinging = 7,
            Disconnecting = 8,
            Disconnected = 9,

            Count = 10,
        };

        // public life cycle managment
    public:
        ~MqttClient();
        MqttClient(const MqttClient&);
        MqttClient(MqttClient&&);
        MqttClient& operator=(const MqttClient&);
        MqttClient& operator=(MqttClient&&);

        // public Methods
    public:
        /**
         * This is the default constructor for the MqttClient class.
         * It initializes the client with no dependencies.
         */
        MqttClient(const char* clientID, Storage::MessageReceived* callback,
                   const Storage::DynamicBinaryDataView* brokerCert, Storage::PacketStore* storage);

        /**
         * This method will set up the client with its dependencies,
         * preparing it to be able to issue requests to servers.
         *
         * @param[in] deps
         *     These are all of the configuration items and dependency objects
         *     needed by the client when it's mobilized.
         */
        void Mobilize(const MqttMobilizationDependencies& deps);

        /**
         * This method stops processing of server connections,
         * and releases the transport layer, returning the client back to the
         * state it was in before Mobilize was called.
         */
        void Demobilize();

        /**
         * This method connects to the given broker using the given
         * parameters.
         *
         * @param[in] brokerHost
         *      This is the host name of the broker to connect to.
         * @param[in] port
         *      This is the port number of the broker to connect to.
         * @param[in] useTLS
         *      This indicates if TLS should be used or not.
         * @param[in] cleanSession
         *      This indicates if a clean session should be used or not.
         * @param[in] keepAlive
         *      This is the keep alive time in seconds.
         * @param[in] userName
         *      This is the user name to use for authentication.
         * @param[in] password
         *      This is the password to use for authentication.
         */
        std::shared_ptr<Transaction> ConnectTo(
            const std::string& brokerHost, const uint16_t port, bool useTLS = false,
            const bool cleanSession = true, const uint16_t keepAlive = 300,
            const char* userName = nullptr, const DynamicBinaryData* password = nullptr,
            WillMessage* willMessage = nullptr, const QoSDelivery willQoS = QoSDelivery::AtLeastOne,
            const bool willRetain = false, Properties* properties = nullptr);

        /**
         * This method disconnects from the broker.
         *
         * @param[in] code
         *      This is the reason code to use for the disconnection.
         * @param[in] properties
         *      This is the properties to use for the disconnection.
         */
        std::shared_ptr<Transaction> Disconnect(const ReasonCode code,
                                                Properties* properties = nullptr);

        /**
         * This method disconnects from the broker.
         *
         * @param[in] code
         *      This is the reason code to use for the disconnection.
         * @param[in] properties
         *      This is the properties to use for the disconnection.
         */
        SystemUtils::DiagnosticsSender::UnsubscribeDelegate SubscribeToDiagnostics(
            SystemUtils::DiagnosticsSender::DiagnosticMessageDelegate delegate,
            size_t minLevel = 0) override;

        std::shared_ptr<Transaction> Authenticate(const ReasonCode& reasonCode,
                                                  const DynamicStringView& authMethod,
                                                  const DynamicBinaryDataView& authData,
                                                  Properties* properties = nullptr) override;

        std::shared_ptr<Transaction> Subscribe(
            const char* topic,
            const RetainHandling retainHandling =
                RetainHandling::GetRetainedMessageAtSubscriptionTime,
            const bool withAutoFeedBack = false,
            const QoSDelivery maxAcceptedQos = QoSDelivery::ExactlyOne,
            const bool retainAsPublished = true, Properties* properties = nullptr) override;

        std::shared_ptr<Transaction> Subscribe(SubscribeTopic* topics,
                                               Properties* properties = nullptr) override;

        std::shared_ptr<Transaction> Unsubscribe(UnsubscribeTopic* topics,
                                                 Properties* properties = nullptr) override;

        std::shared_ptr<Transaction> Publish(const std::string topic, const std::string payload,
                                             const bool retain = false,
                                             const QoSDelivery QoS = QoSDelivery::AtMostOne,
                                             const uint16_t packetID = 0,
                                             Properties* properties = nullptr) override;
        /**
         * This is the type of structure that contains the private
         * properties of the instance. It is defined in the implementation
         * and declared here to ensure that it is scoped inside the class.
         */
        struct Impl;

    private:
        /**
         * This contains the private properties of the instance.
         */
        std::shared_ptr<struct Impl> impl_;
    };
}  // namespace MqttV5

#endif /* MQTT_MQTTCLIENT_HPP */