#ifndef MQTTV5_MQTTBROKER_TRANSPORT_LAYER_HPP
#define MQTTV5_MQTTBROKER_TRANSPORT_LAYER_HPP

/**
 * @file TransportLayer.hpp
 *
 * This module declares the Mqtt::ClientTransportLayer interface.
 *
 * © 2025 by Hatem Nabli
 */

#include "MqttV5/Connection.hpp"

#include <stdint.h>
#include <functional>
#include <memory>

namespace MqttV5
{
    /**
     * This represents the transport layer requirements of the
     * MqttClient.To integrate MqttV5::MqttV5Core::MqttClient into a larger application,
     * implement this interfaces in terms of the actual transport layer.
     *
     */
    class ClientTransportLayer
    {
        // Types
    public:
        /**
         * This method establishes a new connection to a server with
         * the given address and port number.
         *
         * @note
         *     The object returned does not do anything for the
         *     SetDataReceivedDelegate or SetBrokenDelegate
         *     methods, since the delegates are specified directly
         *     in this method.
         *
         * @param[in] scheme
         *     This is the scheme indicated in the URI of the target
         *     to which to establish a connection.
         *
         * @param[in] hostNameOrAddress
         *     This is the host name or IP address of the
         *     server to which to connect.
         *
         * @param[in] port
         *     This is the port number of the server to which to connect.
         *
         * @param[in] dataReceivedDelegate
         *     This is the delegate to call whenever data is recevied
         *     from the remote peer.
         *
         * @param[in] dataReceivedDelegate
         *     This is the delegate to call whenever the connection
         *     has been broken.
         *
         * @return
         *     An object representing the new connection is returned.
         *
         * @retval nullptr
         *     This is returned if a connection could not be established.
         */
        virtual std::shared_ptr<Connection> Connect(
            const std::string& scheme, const std::string& hostNameOrAddress, uint16_t port,
            MqttV5::Connection::DataReceivedDelegate dataReceivedDelegate,
            MqttV5::Connection::BrokenDelegate brokenDelegate) = 0;
    };

}  // namespace MqttV5

#endif /* MQTT_MQTTV5_TRANSPORT_LAYER_HPP */