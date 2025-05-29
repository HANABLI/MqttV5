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
#include <stdint.h>
#include <SystemUtils/DiagnosticsSender.hpp>
#include "MqttV5/MqttV5Packets/MqttV5Packets.hpp"
#include "ClientTransportLayer.hpp"
#include "TimeKeeper.hpp"
#include "MqttV5/MqttV5Types.hpp"
#include "MqttV5/MqttV5Properties.hpp"

namespace MqttV5::Storage
{
    typedef MqttV5::Common::LocalError LocalError;
    typedef MqttV5::Mqtt_V5::DynamicBinaryData DynamicBinaryData;
    typedef MqttV5::Mqtt_V5::DynamicBinaryDataView DynamicBinaryDataView;
    typedef MqttV5::Mqtt_V5::DynamicString DynamicString;
    typedef MqttV5::Mqtt_V5::DynamicStringPair DynamicStringPair;
    typedef MqttV5::Mqtt_V5::DynamicStringView DynamicStringView;
    typedef MqttV5::Properties Properties;

    class PacketStore
    {
    public:
        virtual bool StorePacket(uint32_t packetID, uint32_t packetSize, const uint8_t* packet) = 0;
        // virtual bool RetrievePacket(uint32_t packetID, uint32_t& packetSize, uint8_t*& packet) =
        // 0;
        virtual bool RetrievePacket(uint32_t packetID, uint32_t& headSize, uint8_t*& headPacke,
                                    uint32_t tailSize, uint8_t*& tailPacket) = 0;
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
                            uint32_t tailSize, uint8_t*& tailPacket);
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
        virtual void onMessageReceived(const DynamicStringView& topic,
                                       DynamicBinaryDataView& payload, uint16_t packetId,
                                       Properties& properties) = 0;
        virtual uint32_t maxPacketSize() const { return 2048U; }   // Default max packet size;
        virtual uint32_t maxUnAckedPackets() const { return 1U; }  // Default max unacked packets;
        virtual bool onConnectionLost(const ReasonCode& reasonCode) = 0;
        virtual ~MessageReceived() = default;
    };
}  // namespace MqttV5::Storage
namespace MqttV5
{
    class MqttClient
    {
        /**
         * This structure holds all of the configuration items
         * and dependency objects needed by the client when it's
         * mobilized.
         */
        struct MobilizationDependencies
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
        };

        struct Transaction
        {
            enum class State : int32_t
            {
                Success = 0,            //!< The method succeeded as expected
                TimedOut = -2,          //!< The operation timed out
                AlreadyConnected = -3,  //!< Already connected to a server
                BadParameter = -4,      //!< Bad parameter for this method call
                BadProperties =
                    -5,  //!< The given properties are unexpected (no packet was sent yet)
                NetworkError = -6,  //!< A communication with the network failed
                NotConnected = -7,  //!< Not connected to the server
                TranscientPacket =
                    -8,  //!< A transcient packet was captured and need to be processed first
                WaitingForResult =
                    -9,  //!< The available answer is not ready yet, need to call again later on
                StorageError = -10,  //!< Can't store the value as expected

                UnknownError = -1,  //!< An unknown error happened (or the developer was too lazy to
                                    //!< create a valid entry in this table...
            };

            State responseState;  //!< The response state of the transaction

            State State() const { return responseState; }
        };
        // public life cycle managment
    public:
        ~MqttClient();
        MqttClient(const MqttClient&);
        MqttClient(MqttClient&&);
        MqttClient operator=(const MqttClient&);
        MqttClient& operator=(MqttClient&&);

        // public Methods
    public:
        /**
         * This is the default constructor for the MqttClient class.
         * It initializes the client with no dependencies.
         */
        MqttClient();

        /**
         * This method will set up the client with its dependencies,
         * preparing it to be able to issue requests to servers.
         *
         * @param[in] deps
         *     These are all of the configuration items and dependency objects
         *     needed by the client when it's mobilized.
         */
        void Mobilize(const MobilizationDependencies& deps);

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
        void ConnectTo(const char* brokerHost, const uint16_t port, bool useTLS = false,
                       const bool cleanSession = true, const uint16_t keepAlive = 300,
                       const char* userName = nullptr,
                       const DynamicBinaryDataView* password = nullptr,
                       WillMessage* willMessage = nullptr,
                       const QoSDelivery willQoS = QoSDelivery::AtLeastOne,
                       const bool willRetain = false, Properties* properties = nullptr);
        /**
         * This method authenticates the client to the broker using the given
         * authentication method and data.
         *
         * @param[in] reasonCode
         *      This is the reason code to use for the authentication.
         * @param[in] authMethod
         *      This is the authentication method to use.
         * @param[in] authData
         *     This is the authentication data to use.
         * @param[in] properties
         *     This is the properties to use for the authentication.
         */
        void authenticate(const ReasonCode& reasonCode, const DynamicStringView& authMethod,
                          const DynamicBinaryDataView& authData, Properties* properties = nullptr);

        /**
         * This method subscribes to the given topic.
         *
         * @param[in] topic
         *      This is the topic to subscribe to.
         * @param[in] retainHandling
         *      This is the retain handling to use for the subscription.
         * @param[in] withAutoFeedBack
         *     This indicates if the subscription should be
         *    automatically fed back to the broker or not.
         * @param[in] maxAcceptedQos
         *      This is the maximum accepted QoS level for the subscription.
         * * @param[in] retainAsPublished
         *      This indicates if the subscription should be retained as published or not.
         * @param[in] properties
         *      This is the properties to use for the subscription.
         */
        void Subscribe(const char* topic,
                       const RetainHandling retainHandling =
                           RetainHandling::GetRetainedMessageAtSubscriptionTime,
                       const bool withAutoFeedBack = false,
                       const QoSDelivery maxAcceptedQos = QoSDelivery::ExactlyOne,
                       const bool retainAsPublished = true, Properties* properties = nullptr);
        /**
         * This method subscribes to the given topics.
         *
         * @param[in] topic
         *      This is the topics to subscribe to.
         * @param[in] properties
         *      This is the properties to use for the subscription.
         */
        void Subscribe(SubscribeTopic& topics, Properties* properties = nullptr);
        /**
         * This method unsubscribes from the given topics.
         *
         * @param[in] topic
         *      This is the topics to unsubscribe from.
         * @param[in] properties
         *      This is the properties to use for the unsubscription.
         */
        void Unsubscribe(SubscribeTopic& topics, Properties* properties = nullptr);
        /**
         * This method publishes the given payload to the given topic.
         *
         * @param[in] topic
         *      This is the topic to publish to.
         * @param[in] payload
         *      This is the payload to publish.
         * @param[in] retain
         *      This indicates if the message should be retained or not.
         * @param[in] QoS
         *      This is the QoS level to use for the publication.
         * @param[in] packetID
         *      This is the packet ID to use for the publication.
         * @param[in] properties
         *      This is the properties to use for the publication.
         */
        void Publish(const char* topic, const char* payload, const bool retain = false,
                     const QoSDelivery QoS = QoSDelivery::AtMostOne, const uint16_t packetID = 0,
                     Properties* properties = nullptr);

        /**
         * This method disconnects from the broker.
         *
         * @param[in] code
         *      This is the reason code to use for the disconnection.
         * @param[in] properties
         *      This is the properties to use for the disconnection.
         */
        void Disconnect(const ReasonCode code, Properties* properties = nullptr);

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
            size_t minLevel = 0);

    private:
        /**
         * This is the type of structure that contains the private
         * properties of the instance. It is defined in the implementation
         * and declared here to ensure that it is scoped inside the class.
         */
        struct Impl;
        /**
         * This contains the private properties of the instance.
         */
        std::unique_ptr<struct Impl> impl_;
    };
}  // namespace MqttV5

#endif /* MQTT_MQTTCLIENT_HPP */