#ifndef I_MQTT_V5_CLIENT_HPP
#define I_MQTT_V5_CLIENT_HPP
#include "MqttV5ISerializable.hpp"
#include "Connection.hpp"
#include "MqttV5Properties.hpp"
#include "MqttV5Packets.hpp"
#include "MqttV5Types.hpp"
#include "MqttV5Constants.hpp"
#include <SystemUtils/DiagnosticsSender.hpp>
#include <functional>
#include <memory>

namespace MqttV5
{
    class IMqttV5Client
    {
    public:
        struct Transaction
        {
            enum class State
            {
                Success = 0,            //!< The method succeeded as expected
                TimedOut = -2,          //!< The operation timed out
                AlreadyConnected = -3,  //!< Already connected to a server
                BadParameter = -4,      //!< Bad parameter for this method call
                //!< The given properties are unexpected (no packet was sent yet)
                BadProperties = -5,
                NetworkError = -6,  //!< A communication with the network failed
                NotConnected = -7,  //!< Not connected to the server
                //!< A transcient packet was captured and need to be processed first
                TranscientPacket = -8,
                //!< The available answer is not ready yet, need to call again later on
                WaitingForResult = -9,
                StorageError = -10,  //!< Can't store the value as expected
                //!< A Shundek Packet was received or packet construction error
                ShunkedPacket = -11,
                UnknownError = -1,  //!< An unknown error happened (or the developer was too lazy to
                                    //!< create a valid entry in this table...
            };

            /**
             * This indicate the state of the transaction
             */
            State transactionState = State::NotConnected;

            /**
             * This indicate the packetID handled by the transaction
             */
            uint16_t packetID;

            /**
             * This method is used to wait for the transaction to complete.
             *
             * @note
             *      This method will return immediately if the state is not
             *      State::WaitingForResult.
             * @param[in] relativeTime
             *      This is the maximum amount of time, in milliseconds,
             *      to wait for the transaction to complete.
             * @return
             *      An indication of whether or not the transaction was complete
             *      in time is returned.
             */
            virtual bool AwaitCompletion(const std::chrono::milliseconds& relativeTime) = 0;

            /**
             * This method is used to wait for the transaction to complete.
             *
             * @note
             *      This method will return immediately if the state is not State::InProgress.
             */
            virtual void AwaitCompletion() = 0;

            /**
             * Set a delegate to be called once the transaction is completed.
             *
             * @param[in] completionDelegate
             *      This is the delegate to call once the transaction is completed.
             */
            virtual void SetCompletionDelegate(
                std::function<void(std::vector<ReasonCode>&)> completionDelegate) = 0;
        };

        virtual SystemUtils::DiagnosticsSender::UnsubscribeDelegate SubscribeToDiagnostics(
            SystemUtils::DiagnosticsSender::DiagnosticMessageDelegate delegate,
            size_t minLevel = 0) = 0;

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
        virtual std::shared_ptr<Transaction> Authenticate(const ReasonCode& reasonCode,
                                                          const DynamicStringView& authMethod,
                                                          const DynamicBinaryDataView& authData,
                                                          Properties* properties) = 0;

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
        virtual std::shared_ptr<Transaction> Subscribe(
            const std::string& brokerId, const char* topic, const RetainHandling retainHandling,
            const bool withAutoFeedBack, const QoSDelivery maxAcceptedQos,
            const bool retainAsPublished, Properties* properties) = 0;
        /**
         * This method subscribes to the given topics.
         *
         * @param[in] topic
         *      This is the topics to subscribe to.
         * @param[in] properties
         *      This is the properties to use for the subscription.
         */
        virtual std::shared_ptr<Transaction> Subscribe(const std::string& brokerId,
                                                       SubscribeTopic* topics,
                                                       Properties* properties) = 0;
        /**
         * This method unsubscribes from the given topics.
         *
         * @param[in] topic
         *      This is the topics to unsubscribe from.
         * @param[in] properties
         *      This is the properties to use for the unsubscription.
         */
        virtual std::shared_ptr<Transaction> Unsubscribe(UnsubscribeTopic* topics,
                                                         Properties* properties) = 0;
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
        virtual std::shared_ptr<Transaction> Publish(const std::string& brokerId,
                                                     const std::string topic,
                                                     const std::string payload, const bool retain,
                                                     const QoSDelivery QoS, const uint16_t packetID,
                                                     Properties* properties) = 0;
    };
}  // namespace MqttV5

#endif /* I_MQTT_V5_CLIENT_HPP */