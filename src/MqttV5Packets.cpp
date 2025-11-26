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
        connectPacket->computePacketSize(true);
        return connectPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildPublishPacket(
        const uint16_t packetID, const std::string topicName, const std::vector<uint8_t> payload,
        const uint32_t payloadSize, const QoSDelivery qos, const bool retain,
        Properties* properties) {
        // Implementation of the buildPublishPacket function
        PublishPacket* publishPacket = new PublishPacket();

        if (properties)
        { publishPacket->props.captureProperties(*properties); }
        uint8_t* payloadBuff = new uint8_t[payload.size()];
        std::copy(payload.begin(), payload.end(), payloadBuff);
        publishPacket->header.setRetained(retain);
        publishPacket->header.setDuplicated(false);
        publishPacket->header.setQos(qos);
        publishPacket->fixedVariableHeader.packetID = packetID;
        publishPacket->fixedVariableHeader.topicName = topicName;
        publishPacket->payload.setExpectedPacketSize(payloadSize);
        publishPacket->payload.deserialize(payloadBuff, payloadSize);
        publishPacket->computePacketSize(true);
        delete[] payloadBuff;
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

        if (properties)
        { subscribePacket->props.captureProperties(*properties); }

        subscribePacket->fixedVariableHeader.packetID = packetID;
        subscribePacket->payload.topicList = topicList;
        subscribePacket->computePacketSize(true);
        return subscribePacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildUnsubscribePacket(const uint16_t packetID,
                                                                      UnsubscribeTopic* topicList,
                                                                      Properties* properties) {
        // Implementation of the buildUnsubscribePacket function
        UnsubscribePacket* unsubscribePacket = new UnsubscribePacket();

        if (properties)
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

    ControlPacketSerializable* PacketsBuilder::buildConnAckPacket(const uint8_t reasonCode,
                                                                  Properties* properties) {
        ConnAckPacket* connAckPacket = new ConnAckPacket();
        if (reasonCode)
        { connAckPacket->fixedVariableHeader.reasonCode = reasonCode; }
        if (properties)
        { connAckPacket->props.captureProperties(*properties); }
        connAckPacket->computePacketSize(true);
        return connAckPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildSubAckPacket(const uint16_t packetId,
                                                                 const uint8_t reasonCode,
                                                                 Properties* properties) {
        SubAckPacket* subAckPacket = new SubAckPacket();
        subAckPacket->fixedVariableHeader.packetID = packetId;
        uint8_t* rc = new uint8_t[1];
        rc[0] = reasonCode;
        subAckPacket->payload.data = rc;
        if (properties)
        { subAckPacket->props.captureProperties(*properties); }
        subAckPacket->payload.setExpectedPacketSize(1);
        subAckPacket->computePacketSize(true);
        return subAckPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildSubAckPacketMultiTopics(
        const uint16_t packetId, std::vector<uint8_t> reasons, Properties* properties) {
        SubAckPacket* subAckPacket = new SubAckPacket();
        subAckPacket->fixedVariableHeader.packetID = packetId;
        uint8_t* buffer = new uint8_t[reasons.size()];
        std::copy(reasons.begin(), reasons.end(), buffer);
        auto reasonsSize = subAckPacket->payload.deserialize(buffer, (uint32_t)reasons.size());
        delete[] buffer;
        if (properties)
        { subAckPacket->props.captureProperties(*properties); }
        subAckPacket->payload.setExpectedPacketSize(reasonsSize);
        subAckPacket->computePacketSize(true);
        return subAckPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildUnsubAckPacket(const uint16_t packetId,
                                                                   std::vector<uint8_t> reasons,
                                                                   Properties* properties) {
        UnsubAckPacket* unSubAckPacket = new UnsubAckPacket();
        unSubAckPacket->fixedVariableHeader.packetID = packetId;
        uint8_t* buffer = new uint8_t[reasons.size()];
        std::copy(reasons.begin(), reasons.end(), buffer);
        auto reasonsSize = unSubAckPacket->payload.deserialize(buffer, (uint32_t)reasons.size());
        delete[] buffer;
        if (properties)
        { unSubAckPacket->props.captureProperties(*properties); }
        unSubAckPacket->payload.setExpectedPacketSize(reasonsSize);
        unSubAckPacket->computePacketSize(true);
        return unSubAckPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildPubAckPacket(const uint16_t packetId,
                                                                 const uint8_t reasonCode,
                                                                 Properties* properties) {
        PubAckPacket* pubAckPacket = new PubAckPacket();
        pubAckPacket->fixedVariableHeader.packetID = packetId;
        pubAckPacket->fixedVariableHeader.reasonCode = reasonCode;
        if (properties)
        { pubAckPacket->props.captureProperties(*properties); }
        pubAckPacket->computePacketSize(true);
        return pubAckPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildPubRelPacket(const uint16_t packetId,
                                                                 const uint8_t reasonCode,
                                                                 Properties* properties) {
        PubRelPacket* pubRelPacket = new PubRelPacket();
        pubRelPacket->fixedVariableHeader.packetID = packetId;
        pubRelPacket->fixedVariableHeader.reasonCode = reasonCode;
        if (properties)
        { pubRelPacket->props.captureProperties(*properties); }
        pubRelPacket->computePacketSize(true);
        return pubRelPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildPubRecPacket(const uint16_t packetId,
                                                                 const uint8_t reasonCode,
                                                                 Properties* properties) {
        PubRecPacket* pubRelPacket = new PubRecPacket();
        pubRelPacket->fixedVariableHeader.packetID = packetId;
        pubRelPacket->fixedVariableHeader.reasonCode = reasonCode;
        if (properties)
        { pubRelPacket->props.captureProperties(*properties); }
        pubRelPacket->computePacketSize(true);
        return pubRelPacket;
    }

    ControlPacketSerializable* PacketsBuilder::buildPubCompPacket(const uint16_t packetId,
                                                                  const uint8_t reasonCode,
                                                                  Properties* properties) {
        PubCompPacket* pubCompPacket = new PubCompPacket();
        pubCompPacket->fixedVariableHeader.packetID = packetId;
        pubCompPacket->fixedVariableHeader.reasonCode = reasonCode;
        if (properties)
        { pubCompPacket->props.captureProperties(*properties); }
        pubCompPacket->computePacketSize(true);
        return pubCompPacket;
    }

}  // namespace MqttV5
