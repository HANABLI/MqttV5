/**
 * @file MqttV5Packets.cpp
 * @brief This file contains the implementation of the MQTT V5 packets.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 * @note This file is part of the MqttV5 library.
 */

#include "MqttV5/MqttV5Packets.hpp"

namespace MqttV5
{
    using namespace Common;
    using namespace Mqtt_V5;

    ControlPacketSerializable* PacketsBuilder::buildConnectPacket(
        const char* clientId, const char* username, const DynamicBinaryData* password,
        bool cleanSession, uint16_t keepAlive, WillMessage* willMessage, const QoSDelivery willQoS,
        const bool willRetain, Properties* properties) {
        // Implementation of the buildConnectPacket function

        ConnectPacket* connectPacket = new ConnectPacket();

        auto packetSizeMax = MaximumPacketSize_prop::create(65535);
        auto receiveMax = ReceiveMaximum_prop::create(8UL / 3);

        if (properties != nullptr)
        {
            if (connectPacket->props.captureProperties(*properties))
            {
                connectPacket->props.addProperty(packetSizeMax);
                connectPacket->props.addProperty(receiveMax);
            }
        }

        // Header object
        connectPacket->fixedVariableHeader.keepAlive = keepAlive;
        connectPacket->fixedVariableHeader.cleanSession = cleanSession;
        connectPacket->fixedVariableHeader.willFlag = willMessage != nullptr ? 1 : 0;
        connectPacket->fixedVariableHeader.willQoS = (uint8_t)willQoS;
        connectPacket->fixedVariableHeader.willRetain = willRetain;
        connectPacket->fixedVariableHeader.userName = username != nullptr ? 1 : 0;
        connectPacket->fixedVariableHeader.password =
            password != nullptr && password->size > 0 ? 1 : 0;
        // Payload object

        connectPacket->payload.clientID = clientId;
        if (willMessage != nullptr)
            connectPacket->payload.setWillMessage(willMessage);
        if (username != nullptr)
            connectPacket->payload.userName = username;
        if (password != nullptr)
            connectPacket->payload.password = *password;

        return connectPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildPublishPacket(
        const uint16_t packetID, const char* topicName, const uint8_t* payload,
        const uint32_t payloadSize, const QoSDelivery qos, const bool retain,
        Properties* properties) {
        // Implementation of the buildPublishPacket function
        PublishPacket* publishPacket = new PublishPacket();

        if (properties != nullptr)
        { publishPacket->props.captureProperties(*properties); }

        publishPacket->header.setRetained(retain);
        publishPacket->header.setDuplicated(false);
        publishPacket->header.setQos(qos);
        publishPacket->fixedVariableHeader.packetID = packetID;
        publishPacket->fixedVariableHeader.topicName = topicName;
        publishPacket->payload.setExpectedPacketSize(payloadSize);
        publishPacket->payload.deserialize(payload, payloadSize);

        return publishPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildSubscribePacket(
        const uint16_t packetID, const char* topic, const RetainHandling retainHandling,
        const bool nonlocal, const QoSDelivery maxAcceptedQos, const bool retainAsPublished,
        Properties* properties) {
        // Implementation of the buildSubscribePacket function
        SubscribeTopic* topicList =
            new SubscribeTopic(topic, retainHandling, retainAsPublished, nonlocal, maxAcceptedQos);

        return buildSubscribePacket(packetID, topicList, properties);
    }

    ControlPacketSerializable* PacketsBuilder::buildSubscribePacket(const uint16_t packetID,
                                                                    SubscribeTopic* topicList,
                                                                    Properties* properties) {
        // Implementation of the buildSubscribePacket function
        SubscribePacket* subscribePacket = new SubscribePacket();

        if (properties != nullptr)
        { subscribePacket->props.captureProperties(*properties); }

        subscribePacket->fixedVariableHeader.packetID = packetID;
        subscribePacket->payload.topicList = topicList;

        return subscribePacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildUnsubscribePacket(const uint16_t packetID,
                                                                      UnsubscribeTopic* topicList,
                                                                      Properties* properties) {
        // Implementation of the buildUnsubscribePacket function
        UnsubscribePacket* unsubscribePacket = new UnsubscribePacket();

        if (properties != nullptr)
        { unsubscribePacket->props.captureProperties(*properties); }

        unsubscribePacket->payload.topicList = topicList;
        unsubscribePacket->fixedVariableHeader.packetID = packetID;

        return unsubscribePacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildPingPacket() {
        // Implementation of the buildPingPacket function
        PingReqPacket* pingPacket = new PingReqPacket();
        return pingPacket;
    }

}  // namespace MqttV5
