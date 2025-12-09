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

    std::shared_ptr<ControlPacketSerializable> PacketsBuilder::buildConnectPacket(
        const char* clientId, const char* username, const DynamicBinaryData* password,
        bool cleanSession, uint16_t keepAlive, WillMessage* willMessage, const QoSDelivery willQoS,
        const bool willRetain, Properties* properties) {
        // Implementation of the buildConnectPacket function

        auto connectPacket = std::make_shared<ConnectPacket>();

        // --- Fixed / Variable header ---

        auto& vh = connectPacket->fixedVariableHeader;

        vh.keepAlive = keepAlive;
        vh.cleanSession = cleanSession ? 1 : 0;

        // Will flags
        if (willMessage->payload.size > 0)
        {
            vh.willFlag = 1;
            vh.willQoS = static_cast<uint8_t>(willQoS);
            vh.willRetain = willRetain ? 1 : 0;
        } else
        {
            vh.willFlag = 0;
            vh.willQoS = 0;
            vh.willRetain = 0;
        }

        // Username / Password flags (attention, MQTT regarde juste ces bits)
        const bool hasUserName = (username != nullptr && username[0] != '\0');

        bool hasPassword = false;
        if (password != nullptr)
        {
            // adapte ces champs à ta vraie struct DynamicBinaryData
            hasPassword = (password->data != nullptr && password->size > 0);
        }

        vh.userName = hasUserName ? 1 : 0;
        vh.password = hasPassword ? 1 : 0;

        // --- Propriétés MQTT v5 (variable header) ---
        if (properties != nullptr)
        {
            // en supposant que ConnectPacket a un membre .props compatible
            connectPacket->props.captureProperties(*properties);
        }

        // --- Payload ---

        // Client ID (obligatoire en v5 si ce n’est pas un mode spécial)
        connectPacket->payload.clientID = clientId ? clientId : "";

        // Will message
        if (willMessage)
        { connectPacket->payload.setWillMessage(willMessage); }

        // Username (string)
        if (hasUserName)
        { connectPacket->payload.userName = username; }

        // Password (binary)
        if (hasPassword)
        {
            connectPacket->payload.password = *password;  // copie la struct
        }

        // Calculer Remaining Length / tailles internes
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
        pingPacket->computePacketSize(true);
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
