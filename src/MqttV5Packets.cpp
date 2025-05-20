/**
 * @file MqttV5Packets.cpp
 * @brief This file contains the implementation of the MQTT V5 packets.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 * @note This file is part of the MqttV5 library.
 */

#include "MqttV5/MqttV5Packets/MqttV5Packets.hpp"
#include "MqttV5/MqttV5Constants.hpp"

namespace MqttV5
{
    using namespace Common;
    using namespace Mqtt_V5;

    ControlPacketSerializable* PacketsBuilder::buildConnectPacket(
        const char* clientId, const char* username, const DynamicBinaryData* password,
        bool cleanSession, uint16_t keepAlive, WillMessage* willMessage, const QoSDelivery willQoS,
        const bool willRetain, Properties* properties) {
        // Implementation of the buildConnectPacket function
        uint8_t* packet = nullptr;

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

}  // namespace MqttV5
