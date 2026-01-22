#ifndef MQTTV5_CONSTANTS_HPP
#define MQTTV5_CONSTANTS_HPP
/**
 * @file MqttV5Constants.hpp
 * @brief This file contains the definition of the constants for MQTT V5.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */
#pragma once
#include <cstddef>  // for size_t
#include <cstdint>

namespace MqttV5
{
    // ------------------------ Control Packet Types ------------------------
    enum ControlPacketType : uint8_t
    {
        CONNECT = 0x01,
        CONNACK = 0x02,
        PUBLISH = 0x03,
        PUBACK = 0x04,
        PUBREC = 0x05,
        PUBREL = 0x06,
        PUBCOMP = 0x07,
        SUBSCRIBE = 0x08,
        SUBACK = 0x09,
        UNSUBSCRIBE = 0x0A,
        UNSUBACK = 0x0B,
        PINGREQ = 0x0C,
        PINGRESP = 0x0D,
        DISCONNECT = 0x0E,
        AUTH = 0x0F
    };

    // ------------------------ Property ID ------------------------
    enum PropertyId : uint8_t
    {
        BadProperty = 0x00,
        PayloadFormatIndicator = 0x01,
        MessageExpiryInterval = 0x02,
        ContentType = 0x03,
        ResponseTopic = 0x08,
        CorrelationData = 0x09,
        SubscriptionID = 0x0B,
        SessionExpiryInterval = 0x11,
        AssignedClientID = 0x12,
        ServerKeepAlive = 0x13,
        AuthenticationMethod = 0x15,
        AuthenticationData = 0x16,
        RequestProblemInfo = 0x17,
        WillDelayInterval = 0x18,
        RequestResponseInfo = 0x19,
        ResponseInfo = 0x1A,
        ServerReference = 0x1C,
        ReasonString = 0x1F,
        ReceiveMax = 0x21,
        TopicAliasMax = 0x22,
        TopicAlias = 0x23,
        QoSMax = 0x24,
        RetainAvailable = 0x25,
        UserProperty = 0x26,
        PacketSizeMax = 0x27,
        WildcardSubAvailable = 0x28,
        SubscriptionIDAvailable = 0x29,
        SharedSubscriptionAvailable = 0x2A,
        MaxUsedPropertyType
    };

    constexpr int PropertiesCount = 27;

    static const char* getPropertyName(uint8_t propertyType) {
        static const char* propertyMap[PropertiesCount] = {"PayloadFormat",
                                                           "MessageExpiryInterval",
                                                           "ContentType",
                                                           "ResponseTopic",
                                                           "CorrelationData",
                                                           "SubscriptionID",
                                                           "SessionExpiryInterval",
                                                           "AssignedClientID",
                                                           "ServerKeepAlive",
                                                           "AuthenticationMethod",
                                                           "AuthenticationData",
                                                           "RequestProblemInfo",
                                                           "WillDelayInterval",
                                                           "RequestResponseInfo",
                                                           "ResponseInfo",
                                                           "ServerReference",
                                                           "ReasonString",
                                                           "ReceiveMax",
                                                           "TopicAliasMax",
                                                           "TopicAlias",
                                                           "QoSMax",
                                                           "RetainAvailable",
                                                           "UserProperty",
                                                           "PacketSizeMax",
                                                           "WildcardSubAvailable",
                                                           "SubscriptionIDAvailable",
                                                           "SharedSubscriptionAvailable"};
        if (propertyType >= MaxUsedPropertyType)
            return nullptr;

        static const uint8_t invPropertyMap[PropertyId::MaxUsedPropertyType] = {PropertiesCount,
                                                                                0,
                                                                                1,
                                                                                2,
                                                                                PropertiesCount,
                                                                                PropertiesCount,
                                                                                PropertiesCount,
                                                                                PropertiesCount,
                                                                                3,
                                                                                4,
                                                                                PropertiesCount,
                                                                                5,
                                                                                PropertiesCount,
                                                                                PropertiesCount,
                                                                                PropertiesCount,
                                                                                PropertiesCount,
                                                                                PropertiesCount,
                                                                                6,
                                                                                7,
                                                                                8,
                                                                                PropertiesCount,
                                                                                9,
                                                                                10,
                                                                                11,
                                                                                12,
                                                                                13,
                                                                                14,
                                                                                PropertiesCount,
                                                                                15,
                                                                                PropertiesCount,
                                                                                PropertiesCount,
                                                                                16,
                                                                                PropertiesCount,
                                                                                17,
                                                                                18,
                                                                                19,
                                                                                20,
                                                                                21,
                                                                                22,
                                                                                23,
                                                                                24,
                                                                                25,
                                                                                26};

        uint8_t index = invPropertyMap[propertyType];
        if (index == PropertiesCount)
            return nullptr;
        return propertyMap[index];
    }

    // ------------------------ Reason Codes ------------------------
    enum ReasonCode : uint8_t
    {
        Success = 0x00,                              //!< Success
        NormalDisconnection = 0x00,                  //!< Normal disconnection
        GrantedQoS0 = 0x00,                          //!< Granted QoS 0
        GrantedQoS1 = 0x01,                          //!< Granted QoS 1
        GrantedQoS2 = 0x02,                          //!< Granted QoS 2
        DisconnectWithWillMessage = 0x04,            //!< Disconnect with Will Message
        NoMatchingSubscribers = 0x10,                //!< No matching subscribers
        NoSubscriptionExisted = 0x11,                //!< No subscription existed
        ContinueAuthentication = 0x18,               //!< Continue authentication
        ReAuthenticate = 0x19,                       //!< Re-authenticate
        UnspecifiedError = 0x80,                     //!< Unspecified error
        MalformedPacket = 0x81,                      //!< Malformed Packet
        ProtocolError = 0x82,                        //!< Protocol Error
        ImplementationSpecificError = 0x83,          //!< Implementation specific error
        UnsupportedProtocolVersion = 0x84,           //!< Unsupported Protocol Version
        ClientIdentifierNotValid = 0x85,             //!< Client Identifier not valid
        BadUserNameOrPassword = 0x86,                //!< Bad User Name or Password
        NotAuthorized = 0x87,                        //!< Not authorized
        ServerUnavailable = 0x88,                    //!< Server unavailable
        ServerBusy = 0x89,                           //!< Server busy
        Banned = 0x8A,                               //!< Banned
        ServerShuttingDown = 0x8B,                   //!< Server shutting down
        BadAuthenticationMethod = 0x8C,              //!< Bad authentication method
        KeepAliveTimeout = 0x8D,                     //!< Keep Alive timeout
        SessionTakenOver = 0x8E,                     //!< Session taken over
        TopicFilterInvalid = 0x8F,                   //!< Topic Filter invalid
        TopicNameInvalid = 0x90,                     //!< Topic Name invalid
        PacketIdentifierInUse = 0x91,                //!< Packet Identifier in use
        PacketIdentifierNotFound = 0x92,             //!< Packet Identifier not found
        ReceiveMaximumExceeded = 0x93,               //!< Receive Maximum exceeded
        TopicAliasInvalid = 0x94,                    //!< Topic Alias invalid
        PacketTooLarge = 0x95,                       //!< Packet too large
        MessageRateTooHigh = 0x96,                   //!< Message rate too high
        QuotaExceeded = 0x97,                        //!< Quota exceeded
        AdministrativeAction = 0x98,                 //!< Administrative action
        PayloadFormatInvalid = 0x99,                 //!< Payload format invalid
        RetainNotSupported = 0x9A,                   //!< Retain not supported
        QoSNotSupported = 0x9B,                      //!< QoS not supported
        UseAnotherServer = 0x9C,                     //!< Use another server
        ServerMoved = 0x9D,                          //!< Server moved
        SharedSubscriptionsNotSupported = 0x9E,      //!< Shared Subscriptions not supported
        ConnectionRateExceeded = 0x9F,               //!< Connection rate exceeded
        MaximumConnectTime = 0xA0,                   //!< Maximum connect time
        SubscriptionIdentifiersNotSupported = 0xA1,  //!< Subscription Identifiers not supported
        WildcardSubscriptionsNotSupported = 0xA2,    //!< Wildcard Subscriptions not supported
    };

    /** The possible value for retain handling in subscribe packet */
    enum RetainHandling : unsigned int
    {
        GetRetainedMessageAtSubscriptionTime =
            0,  //!< Get the retained message at subscription time
        GetRetainedMessageForNewSubscriptionOnly =
            1,                  //!< Get the retained message only for new subscription
        NoRetainedMessage = 2,  //!< Don't get retained message at all
    };

    /** The possible Quality Of Service values */
    enum QoSDelivery : unsigned int
    {
        AtMostOne = 0,   //!< At most one delivery (unsecure sending)
        AtLeastOne = 1,  //!< At least one delivery (could have retransmission)
        ExactlyOne = 2,  //!< Exactly one delivery (longer to send)
    };
}  // namespace MqttV5
#endif  // MQTTV5_CONSTANTS_HPP