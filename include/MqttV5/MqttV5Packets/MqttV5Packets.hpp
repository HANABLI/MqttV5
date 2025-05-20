#ifndef MQTTV5_MQTTV5PACKETS_HPP
#define MQTTV5_MQTTV5PACKETS_HPP
/**
 * @file MqttV5Packets.hpp
 * @brief This file contains the definitions for MQTT V5 packets.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */
#pragma once
#include <Utf8/Utf8.hpp>
#include <vector>
#include "MqttV5/MqttV5Types.hpp"
#include "MqttV5/MqttV5ISerializable.hpp"
#include "MqttV5/MqttV5Properties.hpp"
#include "MqttV5/MqttV5Constants.hpp"
namespace MqttV5
{
    using namespace Common;
    using namespace Mqtt_V5;

    template <ControlPacketType type>
    struct ControlPacketCore
    {
        typedef FixedHeaderType<type, 0> FixedHeader;  //!< Type for the fixed header
        typedef Properties VariableHeader;             //!< Type for the properties
        static const bool hasPayload = false;  //!< Flag to indicate if the packet has a payload
    };

    template <ControlPacketType type>
    struct FixedField : public ISerializable
    {
        virtual void setFlags(const uint8_t&) {}

        virtual void setRemainingLength(const uint32_t) {}

        virtual uint32_t getSerializedSize() const {
            return 0;
        }  //!< Get the size of the serialized object
        virtual uint32_t serialize(uint8_t* buffer) {
            return 0;
        }  //!< Serialize the object into the buffer
        virtual uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) {
            return 0;
        }  //!< Deserialize the object from the buffer
        virtual bool checkImpl() const { return 0; }
    };

    struct FixedFieldGeneric : public ISerializable
    {
        GenericTypeBase& value;  //!< Pointer to the value of the property

        FixedFieldGeneric(GenericTypeBase& v) :
            value(v) {}  //!< Constructor for the FixedFieldGeneric class
        ~FixedFieldGeneric() = default;
        uint32_t getSerializedSize() const override {
            return value.typeSize();  //!< Get the size of the serialized object
        }

        uint32_t serialize(uint8_t* buffer) override {
            if (!buffer)
            {
                return BadData;  // Invalid buffer
            }
            value.swapNetworkOrder();                       // Swap the data to network order
            memcpy(buffer, value.raw(), value.typeSize());  // Copy the data to the buffer
            value.swapNetworkOrder();                       // Swap the data back to host order
            return value.typeSize();                        // Return the size of the object
        }  //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if (bufferSize < value.typeSize())
            {
                return NotEnoughData;  // Not enough data
            }
            memcpy(value.raw(), buffer, value.typeSize());  // Copy the data from the buffer
            value.swapNetworkOrder();                       // Swap the data to host order
            return value.typeSize();                        // Return the size of the object
        }  //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            return value.isValid();  //!< Check the implementation of the object
        }

        virtual void setFlags(const uint8_t&) {}

        virtual void setRemainingLength(const uint32_t) {}
    };

    struct FixedFieldWithRemainingLength : public FixedFieldGeneric
    {
    protected:
        uint32_t remainingLength;  //!< Remaining length of the packet
    public:
        FixedFieldWithRemainingLength(GenericTypeBase& v, uint32_t length) :
            FixedFieldGeneric(v),
            remainingLength(length) {}  //!< Constructor for the FixedFieldWithRemainingLength class
        void setRemainingLength(uint32_t length) {
            remainingLength = length;  //!< Set the remaining length of the packet
        }                              //!< Set the remaining length of the packet
        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            uint32_t offset =
                FixedFieldGeneric::deserialize(buffer, bufferSize);  // Deserialize the fixed field
            if (remainingLength == value.typeSize())
            {
                return Shortcut;  // Return the size of the fixed field
            }
            return offset;
        }  //!< Deserialize the object from the buffer
    };

    struct FixedFieldWithID : public FixedFieldGeneric
    {
        GenericType<uint16_t> packetID;  //!< Packet ID

        FixedFieldWithID() : FixedFieldGeneric(packetID){};
    };

    struct FixedFieldWithReason : public FixedFieldWithRemainingLength
    {
        GenericType<uint8_t> reasonCode;  //!< Reason code

        ReasonCode reason() const {
            return static_cast<ReasonCode>(reasonCode.value);  //!< Get the reason code
        }                                                      //!< Get the reason code

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            uint32_t offset = FixedFieldWithRemainingLength::deserialize(buffer, bufferSize);
            if (remainingLength == 0)
            {
                reasonCode.value = 0;  // Set the reason code
                return Shortcut;       // Return the size of the fixed field
            }
            return offset;
        }  //!< Deserialize the object from the buffer

        FixedFieldWithReason() : FixedFieldWithRemainingLength(reasonCode, 1){};
    };

    struct IDandReasonCode
    {
        uint16_t packetID;   //!< Packet ID
        uint8_t reasonCode;  //!< Reason code

        IDandReasonCode() : packetID(0), reasonCode(0) {}

        IDandReasonCode(uint16_t id, uint8_t reason) :
            packetID(id), reasonCode(reason) {}  //!< Constructor for the IDandReasonCode class
    };

    struct TopicAndID
    {
        /** The topic name */
        DynamicString topicName;
        /** The packet identifier */
        uint16_t packetID;
    };

    struct EmptySerialisable : public ISerializable
    {
        uint32_t getSerializedSize() const override { return 0; }
        //!< Get the size of the serialized object
        uint32_t serialize(uint8_t* buffer) override { return 0; }
        //!< Serialize the object into the buffer
        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            return 0;
        }  //!< Deserialize the object from the buffer
        bool checkImpl() const override {
            return true;
        }  //!< Check the implementation of the object
    };

    struct SerializablePayload : public EmptySerialisable
    {
        virtual void setFlags(const FixedFieldGeneric&) {}

        virtual void setExpectedPacketSize(const uint32_t) {}

        ~SerializablePayload() {}
    };

    template <ControlPacketType type>
    struct Payload : SerializablePayload
    {};

    template <>
    struct ControlPacketCore<ControlPacketType::CONNECT>
    {
        typedef ConnectFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;       //!< Type for the properties
        static const bool hasPayload = true;     //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::CONNACK>
    {
        typedef ConnAckFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;       //!< Type for the properties
        static const bool hasPayload = false;    //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::PUBLISH>
    {
        typedef PublishFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;       //!< Type for the properties
        static const bool hasPayload = true;     //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::PUBACK>
    {
        typedef PubAckFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;      //!< Type for the properties
        static const bool hasPayload = false;   //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::PUBREC>
    {
        typedef PubRecFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;      //!< Type for the properties
        static const bool hasPayload = false;   //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::PUBREL>
    {
        typedef PubRelFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;      //!< Type for the properties
        static const bool hasPayload = false;   //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::PUBCOMP>
    {
        typedef PubCompFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;       //!< Type for the properties
        static const bool hasPayload = false;    //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::SUBSCRIBE>
    {
        typedef SubscribeFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;         //!< Type for the properties
        static const bool hasPayload = true;       //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::SUBACK>
    {
        typedef SubAckFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;      //!< Type for the properties
        static const bool hasPayload = true;    //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::UNSUBSCRIBE>
    {
        typedef UnsubscribeFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;           //!< Type for the properties
        static const bool hasPayload = false;  //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::UNSUBACK>
    {
        typedef UnsubAckFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;        //!< Type for the properties
        static const bool hasPayload = false;     //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::DISCONNECT>
    {
        typedef DisconnectFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;          //!< Type for the properties
        static const bool hasPayload = false;  //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::PINGREQ>
    {
        typedef PingReqFixedHeader FixedHeader;    //!< Type for the fixed field
        typedef EmptySerialisable VariableHeader;  //!< Type for the properties
        static const bool hasPayload = false;      //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::PINGRESP>
    {
        typedef PingRespFixedHeader FixedHeader;   //!< Type for the fixed field
        typedef EmptySerialisable VariableHeader;  //!< Type for the properties
        static const bool hasPayload = false;      //!< Flag to indicate if the packet has a payload
    };
    template <>
    struct ControlPacketCore<ControlPacketType::AUTH>
    {
        typedef AuthFixedHeader FixedHeader;  //!< Type for the fixed field
        typedef Properties VariableHeader;    //!< Type for the properties
        static const bool hasPayload = true;  //!< Flag to indicate if the packet has a payload
    };

    struct ConnectHeaderImpl
    {
        std::vector<uint8_t> protocolName;  //!< Protocol name (MQTT)
        uint8_t protocolLevel;              //!< Protocol level (4 for MQTT 3.1.1)
        union {
            BitField<uint8_t, 0, 1> reserved;      //!< Clean session flag
            BitField<uint8_t, 1, 1> cleanSession;  //!< Clean session flag
            BitField<uint8_t, 2, 1> willFlag;      //!< Will flag
            BitField<uint8_t, 3, 2> willQoS;       //!< Will QoS flag
            BitField<uint8_t, 5, 2> willRetain;    //!< Will QoS flag
            BitField<uint8_t, 6, 1> password;      //!< Password flag
            BitField<uint8_t, 7, 1> userName;      //!< User name flag

        };                   //!< Connect flags
        uint16_t keepAlive;  //!< Keep alive interval

        const std::vector<uint8_t> expectedProtocolName() const {
            static uint8_t name[6] = {0, 4, 'M', 'Q', 'T', 'T'};
            return std::vector<uint8_t>(name, name + sizeof(name));
        }  //!< Expected protocol name

        bool isValid() const {
            return reserved == 0 && willQoS < 3 &&
                   memcmp(protocolName.data(), expectedProtocolName().data(),
                          protocolName.size()) == 0;
        }

        ConnectHeaderImpl() : protocolLevel(5), keepAlive(0) {
            protocolName = expectedProtocolName();
        }
        ~ConnectHeaderImpl() = default;
    };

    struct ConnACKHeaderImpl
    {
        uint8_t acknowledgeFlags;  //!< Acknowledge flags
        uint8_t reasonCode;        //!< Reason code
        bool isValid() const {
            return (acknowledgeFlags & 0xFE) == 0;  //!< Check if the acknowledge flags are valid
        }                                           //!< Check if the ConnACK header is valid

        ConnACKHeaderImpl() :
            acknowledgeFlags(0), reasonCode(0) {}  //!< Default constructor for the ConnACK header
    };

    template <>
    struct GenericType<ConnectHeaderImpl> : public GenericTypeBase
    {
        ConnectHeaderImpl& value;  //!< The value of the type

        GenericType(ConnectHeaderImpl& value) :
            value(value) {}  //!< Constructor for the GenericType class

        uint32_t typeSize() const override { return sizeof(value); }  //!< Get the size of the type

        void swapNetworkOrder() override {
            // Swap the type to network order
            const_cast<ConnectHeaderImpl&>(value).keepAlive = BigEndian(value.keepAlive);
        }  //!< Swap the type to network order
        uint32_t serialize(uint8_t* buffer) const {
            if (!buffer)
            {
                return BadData;  //!< Invalid buffer
            }

            // Serialize the protocol name
            memcpy(buffer, value.protocolName.data(), value.protocolName.size());
            uint32_t offset = (uint32_t)value.protocolName.size();

            // Serialize the protocol level
            buffer[offset++] = value.protocolLevel;

            // Serialize the connect flags
            buffer[offset++] = value.reserved;

            // Serialize the keep-alive interval
            uint16_t keepAliveNetworkOrder = BigEndian(value.keepAlive);
            memcpy(buffer + offset, &keepAliveNetworkOrder, sizeof(keepAliveNetworkOrder));
            offset += sizeof(keepAliveNetworkOrder);

            return offset;  //!< Return the size of the serialized object
        }
        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) {
            if (!buffer || bufferSize < typeSize())
            {
                return NotEnoughData;  //!< Not enough data
            }

            // Deserialize the protocol name
            memcpy(value.protocolName.data(), buffer, value.protocolName.size());
            uint32_t offset = (uint32_t)value.protocolName.size();

            // Deserialize the protocol level
            value.protocolLevel = buffer[offset++];

            // Deserialize the connect flags
            value.reserved = buffer[offset++];

            // Deserialize the keep-alive interval
            uint16_t keepAliveNetworkOrder;
            memcpy(&keepAliveNetworkOrder, buffer + offset, sizeof(keepAliveNetworkOrder));
            value.keepAlive = BigEndian(keepAliveNetworkOrder);
            offset += sizeof(keepAliveNetworkOrder);

            return offset;  //!< Return the size of the deserialized object
        }
        void* raw() override { return &value; }  //!< Get the raw data of the type
        const void* raw() const override { return &value; }
    };

    template <>
    struct GenericType<ConnACKHeaderImpl> : public GenericTypeBase
    {
        ConnACKHeaderImpl& value;  //!< The value of the type

        GenericType<ConnACKHeaderImpl>& operator=(const ConnACKHeaderImpl& o) {
            value = o;
            return *this;
        }
        operator ConnACKHeaderImpl&() {
            return value;
        }  //!< Conversion operator to ConnACKHeaderImpl
        GenericType(ConnACKHeaderImpl& value) :
            value(value) {}  //!< Constructor for the GenericType class

        uint32_t typeSize() const override { return sizeof(value); }  //!< Get the size of the type

        void swapNetworkOrder() override {}  //!< Swap the type to network order

        void* raw() override { return &value; }  //!< Get the raw data of the type
        const void* raw() const override { return &value; }
    };
    template <>
    class FixedField<ControlPacketType::CONNECT> final : public FixedFieldGeneric,
                                                         public ConnectHeaderImpl
    {
        GenericType<ConnectHeaderImpl> value;  //!< The value of the type

    public:
        FixedField() :
            FixedFieldGeneric(value),
            value(*this) {}  //!< Default constructor for the FixedField class
    };

    template <>
    class FixedField<ControlPacketType::CONNACK> final : public FixedFieldGeneric,
                                                         public ConnACKHeaderImpl
    {
        GenericType<ConnACKHeaderImpl> value;  //!< The value of the type

    public:
        FixedField() :
            FixedFieldGeneric(value),
            value(*this) {}  //!< Default constructor for the FixedField class
    };

    template <>
    struct GenericType<IDandReasonCode> final : public GenericTypeBase
    {
        IDandReasonCode& value;  //!< The value of the type

        GenericType(IDandReasonCode& value) :
            value(value) {}  //!< Constructor for the GenericType class

        uint32_t typeSize() const override { return sizeof(value); }  //!< Get the size of the type

        void swapNetworkOrder() override {
            // Swap the type to network order
            const_cast<uint16_t&>(value.packetID) = BigEndian(value.packetID);
        }  //!< Swap the type to network order

        void* raw() override { return &value; }  //!< Get the raw data of the type
        const void* raw() const override { return &value; }
    };

    class FixedFieldWithIDAndReasonCode : public IDandReasonCode,
                                          public FixedFieldWithRemainingLength

    {
        GenericType<IDandReasonCode> value;  //!< The value of the type

    public:
        FixedFieldWithIDAndReasonCode() :
            FixedFieldWithRemainingLength(value, 3),
            value(*this) {}  //!< Default constructor for the FixedField class

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if (bufferSize < 2)
                return NotEnoughData;                     // Not enough data
            memcpy(&packetID, buffer, sizeof(packetID));  // Copy the packet ID from the buffer
            packetID = BigEndian(packetID);               // Swap the packet ID to host order
            if (remainingLength == 0)
            {
                reasonCode = 0;   // Set the reason code
                return Shortcut;  // Return the size of the fixed field
            }
            value.value.reasonCode = buffer[2];  // Copy the reason code from the buffer
            if (remainingLength == 1)
            {
                reasonCode = buffer[2];  // Set the reason code
                return Shortcut;         // Return the size of the fixed field
            }
            return 3;  // Return the size of the fixed field
        }              //!< Deserialize the object from the buffer
    };

    template <>
    struct FixedField<ControlPacketType::SUBSCRIBE> final : public FixedFieldWithID
    {};
    template <>
    struct FixedField<ControlPacketType::UNSUBSCRIBE> final : public FixedFieldWithID
    {};
    template <>
    struct FixedField<ControlPacketType::PUBACK> final : public FixedFieldWithIDAndReasonCode
    {
        uint32_t getSerializedSize() const override {
            return FixedFieldWithIDAndReasonCode::getSerializedSize();
        }

        uint32_t serialize(uint8_t* buffer) override {
            return FixedFieldWithIDAndReasonCode::serialize(buffer);
        }

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            return FixedFieldWithIDAndReasonCode::deserialize(buffer, bufferSize);
        }

        bool checkImpl() const override { return true; }
    };

    template <>
    struct FixedField<ControlPacketType::PUBREC> final : public FixedFieldWithIDAndReasonCode
    {};
    template <>
    struct FixedField<ControlPacketType::PUBREL> final : public FixedFieldWithIDAndReasonCode
    {};
    template <>
    struct FixedField<ControlPacketType::PUBCOMP> final : public FixedFieldWithIDAndReasonCode
    {};
    template <>
    struct FixedField<ControlPacketType::SUBACK> final : public FixedFieldWithID
    {};
    template <>
    struct FixedField<ControlPacketType::UNSUBACK> final : public FixedFieldWithID
    {};
    template <>
    struct FixedField<ControlPacketType::DISCONNECT> final : public FixedFieldWithReason
    {
        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            uint32_t offset = FixedFieldWithReason::deserialize(
                buffer, bufferSize);  // Deserialize the fixed field
            if (offset && remainingLength == 1)
            {
                reasonCode = 0;   // Set the reason code
                return Shortcut;  // Return the size of the fixed field
            }
            return offset;
        }  //!< Deserialize the object from the buffer
    };
    template <>
    struct FixedField<ControlPacketType::AUTH> final : public FixedFieldWithReason
    {};

    template <>
    struct GenericType<TopicAndID> final : public GenericTypeBase
    {
        TopicAndID value;  //!< The value of the type

        GenericType<TopicAndID>& operator=(const TopicAndID& o) {
            value = o;
            return *this;
        }
        operator TopicAndID&() { return value; }         //!< Conversion operator to TopicAndID
        GenericType(TopicAndID value) : value(value) {}  //!< Constructor for the GenericType class

        uint32_t typeSize() const override {
            return value.topicName.getSerializedSize();
        }  //!< Get the size of the type

        void swapNetworkOrder() override {
            // Swap the type to network order
            const_cast<uint16_t&>(value.packetID) = BigEndian(value.packetID);
        }  //!< Swap the type to network order

        bool isValid() const {
            return value.topicName.checkImpl();  //!< Check if the topic name is valid
        }                                        //!< Check if the topic name is valid
        void* raw() override { return &value; }  //!< Get the raw data of the type
        const void* raw() const override { return &value; }
    };

    template <>
    struct FixedField<ControlPacketType::PUBLISH> final : public TopicAndID,
                                                          public FixedFieldGeneric
    {
        GenericType<TopicAndID> value;  //!< The value of the type
    private:
        const uint8_t* flags;  //!< Flags for the fixed field
    public:
        FixedField() :
            FixedFieldGeneric(value),
            value(*this),
            flags(0) {}  //!< Default constructor for the FixedField class

        uint32_t getSerializedSize() const override {
            return topicName.getSerializedSize() + (hasPacketID() ? sizeof(packetID) : 0);
        }  //!< Get the size of the fixed field

        uint32_t serialize(uint8_t* buffer) override {
            uint32_t offset = topicName.serialize(buffer);  // Serialize the topic name
            if (hasPacketID())
            {
                uint16_t p = BigEndian(packetID);
                memcpy(buffer + offset, &p, sizeof(p));  // Copy the packet ID to the buffer
                                                         // Swap the packet ID to network order
                offset += sizeof(packetID);              // Increment the offset
            }
            return offset;  // Return the size of the fixed field
        }                   //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            uint32_t offset =
                topicName.deserialize(buffer, bufferSize);  // Deserialize the fixed field
            if (offset >= Shortcut)
            {
                return offset;  // Return the size of the fixed field
            }
            if (hasPacketID())
            {
                if (bufferSize < offset + sizeof(packetID))
                {
                    return NotEnoughData;  // Not enough data
                }
                memcpy(&packetID, buffer + offset,
                       sizeof(packetID));        // Copy the packet ID from the buffer
                packetID = BigEndian(packetID);  // Swap the packet ID to host order
                offset += sizeof(packetID);      // Increment the offset
            }

            return offset;  // Return the size of the fixed field
        }                   //!< Deserialize the object from the buffer

        inline void setFlags(uint8_t& flags) {
            this->flags = &flags;  //!< Set the flags
        }                          //!< Set the flags

        inline bool hasPacketID() const { return flags && (*flags & 6) > 0; }
    };

    struct WillMessage : ISerializable
    {
        DynamicString topicName;    //!< Topic name of the will message
        DynamicBinaryData payload;  //!< Payload of the will message
        Properties willProperties;  //!< Properties of the will message

        uint32_t getSerializedSize() const override {
            return willProperties.getSerializedSize() + topicName.getSerializedSize() +
                   payload.getSerializedSize();  //!< Get the size of the will message
        }                                        //!< Get the size of the will message

        uint32_t serialize(uint8_t* buffer) override {
            uint32_t offset = willProperties.serialize(buffer);  // Serialize the properties
            offset += topicName.serialize(buffer + offset);      // Serialize the topic name
            offset += payload.serialize(buffer + offset);        // Serialize the payload
            return offset;  // Return the size of the will message
        }                   //!< Serialize the will message into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            uint32_t offset = 0;
            offset += willProperties.deserialize(buffer, bufferSize);  // Deserialize the properties
            offset += topicName.deserialize(buffer + offset,
                                            bufferSize - offset);  // Deserialize the topic name
            offset += payload.deserialize(buffer + offset,
                                          bufferSize - offset);  // Deserialize the payload
            return offset;  // Return the size of the will message
        }                   //!< Deserialize the will message from the buffer

        bool checkImpl() const override {
            if (topicName.checkImpl() && payload.checkImpl() && willProperties.checkImpl())
            {
                return true;  // Check if the will message is valid
            }
            return false;  // Will message is not valid
        }

        WillMessage(const DynamicString&& topicName, const DynamicBinaryData&& payload,
                    const Properties&& willProperties) :
            topicName(topicName),
            payload(payload),
            willProperties(willProperties) {}  //!< Constructor for the WillMessage class
        WillMessage() {}                       //!< Default constructor for the WillMessage class
        WillMessage(const WillMessage& other) :
            topicName(other.topicName),
            payload(other.payload),
            willProperties(other.willProperties) {}  //!< Copy constructor for the WillMessage class
        WillMessage(WillMessage&& other) :
            topicName(std::move(other.topicName)),
            payload(std::move(other.payload)),
            willProperties(std::move(other.willProperties)) {
        }  //!< Move constructor for the WillMessage class
    };

    template <>
    struct Payload<ControlPacketType::CONNECT> final : public SerializablePayload
    {
        DynamicString clientID;      //!< Client ID of the connect packet
        DynamicString userName;      //!< User name of the connect packet
        DynamicBinaryData password;  //!< Password of the connect packet
        WillMessage* willMessage;

        void setWillMessage(WillMessage* msg) { willMessage = msg; }
        void setFlags(const FixedFieldGeneric& flags) {
            fixedHeader =
                (FixedField<ControlPacketType::CONNECT>*)&flags;  //!< Set the fixed header
        }  //!< Set the flags for the connect packet

        bool checkClientID() const {
            // TODO: check allowed charachters in the client id
            return clientID.checkImpl();  //!< Check if the client ID is valid
        }                                 //!< Check if the client ID is valid
        bool checkUserName() const {
            return userName.checkImpl();  //!< Check if the user name is valid
        }                                 //!< Check if the user name is valid
        bool checkPassword() const {
            return password.checkImpl();  //!< Check if the password is valid
        }                                 //!< Check if the password is valid
        bool checkWillMessage() const {
            return willMessage->checkImpl();  //!< Check if the will message is valid
        }                                     //!< Check if the will message is valid

        uint32_t getSerializedSize() const override {
            return clientID.getSerializedSize() + userName.getSerializedSize() +
                   password.getSerializedSize() + willMessage->getSerializedSize() +
                   fixedHeader->getSerializedSize();  //!< Get the size of the payload
        }                                             //!< Get the size of the payload

        uint32_t serialize(uint8_t* buffer) override {
            uint32_t offset = clientID.serialize(buffer);  // Serialize the client ID
            if (!fixedHeader)
                return offset;
            if (fixedHeader->userName)
                offset += userName.serialize(buffer + offset);  // Serialize the user name
            if (fixedHeader->password)
                offset += password.serialize(buffer + offset);  // Serialize the password
            if (fixedHeader->willFlag)
                offset += willMessage->serialize(buffer + offset);  // Serialize the will message
            return offset;  // Return the size of the payload
        }                   //!< Serialize the payload into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            uint32_t offset = clientID.deserialize(buffer,
                                                   bufferSize);  // Deserialize the client ID
            offset += userName.deserialize(buffer + offset,
                                           bufferSize - offset);  // Deserialize the user name
            offset += password.deserialize(buffer + offset,
                                           bufferSize - offset);  // Deserialize the password
            if (!willMessage)
                willMessage = new WillMessage();
            offset +=
                willMessage->deserialize(buffer + offset,
                                         bufferSize - offset);  // Deserialize the will message
            return offset;                                      // Return the size of the payload
        }  //!< Deserialize the payload from the buffer

        bool checkImpl() const override {
            if (checkClientID() && checkUserName() && checkPassword() && checkWillMessage())
            {
                return true;  // Check if the payload is valid
            }
            return false;  // Payload is not valid
        }                  //!< Check if the payload is valid

        Payload(const DynamicString& clientID, const DynamicString& userName,
                const DynamicBinaryData& password, WillMessage* willMessage) :
            clientID(clientID),
            userName(userName),
            password(password),
            willMessage(willMessage) {}  //!< Constructor for the Payload class
        Payload() :
            clientID(),
            userName(),
            password(),
            willMessage(nullptr) {}  //!< Default constructor for the Payload class
        Payload(const Payload& other) :
            clientID(other.clientID),
            userName(other.userName),
            password(other.password),
            willMessage(new WillMessage(*other.willMessage)) {
        }  //!< Copy constructor for the Payload class
        Payload(Payload&& other) :
            clientID(std::move(other.clientID)),
            userName(std::move(other.userName)),
            password(std::move(other.password)),
            willMessage(new WillMessage(std::move(*other.willMessage))) {
        }  //!< Move constructor for the Payload class
        Payload& operator=(const Payload& other) {
            if (this != &other)
            {
                clientID = other.clientID;                          //!< Assign the client ID
                userName = other.userName;                          //!< Assign the user name
                password = other.password;                          //!< Assign the password
                willMessage = new WillMessage(*other.willMessage);  //!< Assign the will message
            }
            return *this;  //!< Return the payload
        }                  //!< Assignment operator for the Payload class
        Payload& operator=(Payload&& other) {
            if (this != &other)
            {
                clientID = std::move(other.clientID);  //!< Move the client ID
                userName = std::move(other.userName);  //!< Move the user name
                password = std::move(other.password);  //!< Move the password
                willMessage =
                    new WillMessage(std::move(*other.willMessage));  //!< Move the will message
            }
            return *this;  //!< Return the payload
        }                  //!< Assignment operator for the Payload class
        ~Payload() {
            if (willMessage != nullptr)
                delete willMessage;  //!< Delete the will message
        }                            //!< Destructor for the Payload class

    private:
        const FixedField<CONNECT>* fixedHeader;
    };

    struct PayloadWithData : public SerializablePayload
    {
        uint8_t* data;      //!< Pointer to the data
        uint32_t dataSize;  //!< Size of the data

        inline void setExpectedPacketSize(const uint32_t size) {
            dataSize = size;  //!< Set the size of the data
        }                     //!< Set the size of the data

        uint32_t getSerializedSize() const override {
            return dataSize;  //!< Get the size of the payload
        }                     //!< Get the size of the payload

        uint32_t serialize(uint8_t* buffer) override {
            if (!buffer)
            {
                return BadData;  // Invalid buffer
            }
            memcpy(buffer, data, dataSize);  // Copy the data to the buffer
            return dataSize;                 // Return the size of the payload
        }                                    //!< Serialize the payload into the buffer
        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if (bufferSize < dataSize)
            {
                return NotEnoughData;  // Not enough data
            }
            memcpy(data, buffer, dataSize);  // Copy the data from the buffer
            return dataSize;                 // Return the size of the payload
        }                                    //!< Deserialize the payload from the buffer

        PayloadWithData(uint8_t* data, uint32_t dataSize) :
            data(data), dataSize(dataSize) {}  //!< Constructor for the PayloadWithData class
        PayloadWithData() :
            data(nullptr), dataSize(0) {}  //!< Default constructor for the PayloadWithData class
        ~PayloadWithData() {
            if (data != nullptr)
                delete[] data;  //!< Delete the data
        }                       //!< Destructor for the PayloadWithData class
    };

    struct SubscribeTopicCore : public ISerializable
    {
        DynamicString topicName;
        SubscribeTopicCore* next = nullptr;

    public:
        bool isValid() const {
            return topicName.checkImpl() &&
                   (next ? next->isValid() : true);  //!< Check if the topic name is valid
        }

        /** Main suicide function */
        void suicide() {
            if (next)
                next->suicide();
            delete this;
        }

        void append(SubscribeTopicCore* newTopic) {
            SubscribeTopicCore** end = &next;
            while (*end)
            { end = &(*end)->next; }
            *end = newTopic;
        }

        uint32_t countTopics() const {
            uint32_t count = 1;  // Start with the current topic
            const SubscribeTopicCore* current = next;
            while (current)
            {
                ++count;
                current = current->next;
            }
            return count;
        }

    public:
        SubscribeTopicCore() {}
        SubscribeTopicCore(const DynamicString& topic) : topicName(topic), next(0) {}
        virtual ~SubscribeTopicCore() { next = 0; }
    };

    struct SubscribeTopic final : public SubscribeTopicCore
    {
        union {
            /** The subscribe option */
            uint8_t option;
            /** Reserved bits, should be 0 */
            BitField<uint8_t, 6, 2> reserved;
            /** The retain policy flag */
            BitField<uint8_t, 4, 2> retainHandling;
            /** The retain as published flag */
            BitField<uint8_t, 3, 1> retainAsPublished;
            /** The non local flag */
            BitField<uint8_t, 2, 1> nonLocal;
            /** The QoS flag */
            BitField<uint8_t, 0, 2> QoS;
        };

        uint32_t getSerializedSize() const override {
            return topicName.getSerializedSize() + 1 +
                   (next ? next->getSerializedSize() : 0);  //!< Get the size of the payload
        }                                                   //!< Get the size of the payload

        uint32_t serialize(uint8_t* buffer) override {
            uint32_t offset = topicName.serialize(buffer);  // Serialize the topic name
            buffer += offset;                               // Increment the offset
            buffer[0] = option;                             // Serialize the option
            offset += 1;                                    // Increment the offset
            if (next)
                offset += next->serialize(buffer + 1);  // Serialize the next topic
            return offset;                              // Return the size of the payload
        }                                               //!< Serialize the payload into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if (next)
                next->suicide();
            next = 0;
            uint32_t offset = 0;
            offset += topicName.deserialize(buffer, bufferSize);  // Deserialize the topic name
            buffer += offset;
            bufferSize -= offset;

            if (!bufferSize)
                return NotEnoughData;  // Invalid data
            option = buffer[0];        // Deserialize the option
            buffer++;
            bufferSize--;
            offset++;
            if (bufferSize)
            {
                next = new SubscribeTopic();
                offset += next->deserialize(buffer, bufferSize);
            }               // Deserialize the next topic
            return offset;  // Return the size of the payload
        }                   //!< Deserialize the payload from the buffer
        bool checkImpl() const override {
            if (reserved == 0 && retainAsPublished != 3 && QoS != 3 &&
                SubscribeTopicCore::isValid())
            {
                return true;  // Check if the payload is valid
            }
            return false;  // Payload is not valid
        }                  //!< Check if the payload is valid

        inline void append(SubscribeTopicCore* newTopic) {
            SubscribeTopicCore::append(newTopic);  //!< Append the new topic to the list
        }                                          //!< Append the new topic to the list

        SubscribeTopic(const DynamicString& topic, const RetainHandling retainedHandling,
                       const bool retainAsPublished, const bool nonLocal, const QoSDelivery QoS) :
            SubscribeTopicCore(topic), option(0) {
            this->retainHandling = retainedHandling;
            this->retainAsPublished = retainAsPublished ? 1 : 0;
            this->nonLocal = nonLocal ? 1 : 0;
            this->QoS = QoS;
        }  //!< Constructor for the SubscribeTopic class

        SubscribeTopic() :
            SubscribeTopicCore() {}  //!< Default constructor for the SubscribeTopic class
    };

    struct UnsubscribeTopic final : public SubscribeTopicCore
    {
        uint32_t getSerializedSize() const override {
            return topicName.getSerializedSize() + 1 +
                   (next ? next->getSerializedSize() : 0);  //!< Get the size of the payload
        }                                                   //!< Get the size of the payload

        uint32_t serialize(uint8_t* buffer) override {
            uint32_t offset = topicName.serialize(buffer);  // Serialize the topic name
            buffer[offset++] = 0;                           // Serialize the option
            if (next)
                offset += next->serialize(buffer + offset);  // Serialize the next topic
            return offset;                                   // Return the size of the payload
        }  //!< Serialize the payload into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            uint32_t offset =
                topicName.deserialize(buffer, bufferSize);  // Deserialize the topic name
            if (offset == BadData)
                return BadData;  // Invalid data
            if (next)
                offset += next->deserialize(buffer + offset,
                                            bufferSize - offset);  // Deserialize the next topic
            return offset;                                         // Return the size of the payload
        }  //!< Deserialize the payload from the buffer

        bool checkImpl() const override { return true; }

        inline void append(SubscribeTopicCore* newTopic) {
            SubscribeTopicCore::append(newTopic);  //!< Append the new topic to the list
        }                                          //!< Append the new topic to the list

        UnsubscribeTopic(DynamicString& topic) : SubscribeTopicCore(topic) {}
        UnsubscribeTopic() : SubscribeTopicCore() {}
    };

    template <>
    struct Payload<ControlPacketType::SUBSCRIBE> final : public SerializablePayload
    {
        SubscribeTopic* topicList;  //!< List of topics to subscribe to

    private:
        uint32_t expectedSize;

    public:
        uint32_t getSerializedSize() const override {
            return topicList ? topicList->getSerializedSize() : 0;
        }  //!< Get the size of the payload

        uint32_t serialize(uint8_t* buffer) override {
            return topicList->serialize(buffer);  // Serialize the payload
        }                                         //!< Serialize the payload into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if (bufferSize < expectedSize)
            {
                return NotEnoughData;  // Not enough data
            }
            if (topicList)
            {
                topicList->suicide();  // Delete the topic list
            }
            topicList = new SubscribeTopic();

            return topicList->deserialize(buffer, expectedSize);
        }  //!< Deserialize the payload from the buffer

        bool checkImpl() const override {
            if (topicList->isValid())
            {
                return true;  // Check if the payload is valid
            }
            return false;  // Payload is not valid
        }                  //!< Check if the payload is valid

        Payload() : topicList(0), expectedSize(0) {}  //!< Constructor for the Payload class
        ~Payload() {
            if (topicList)
                topicList->suicide();
            topicList = 0;
        }
    };

    template <>
    struct Payload<ControlPacketType::UNSUBSCRIBE> final : public SerializablePayload
    {
        UnsubscribeTopic* topicList;  //!< List of topics to unsubscribe from

    private:
        uint32_t expectedSize;

    public:
        uint32_t getSerializedSize() const override {
            return topicList ? topicList->getSerializedSize() : 0;
        }  //!< Get the size of the payload

        uint32_t serialize(uint8_t* buffer) override {
            return topicList->serialize(buffer);  // Serialize the payload
        }                                         //!< Serialize the payload into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if (bufferSize < expectedSize)
            {
                return NotEnoughData;  // Not enough data
            }
            if (topicList)
            {
                topicList->suicide();  // Delete the topic list
            }
            topicList = new UnsubscribeTopic();

            return topicList->deserialize(buffer, expectedSize);
        }  //!< Deserialize the payload from the buffer

        bool checkImpl() const override {
            if (topicList->checkImpl())
            {
                return true;  // Check if the payload is valid
            }
            return false;  // Payload is not valid
        }                  //!< Check if the payload is valid

        Payload() : topicList(0), expectedSize(0) {}  //!< Constructor for the Payload class
        ~Payload() {
            if (topicList)
                topicList->suicide();
            topicList = 0;
        }
    };

    template <>
    struct Payload<ControlPacketType::PUBLISH> final : public PayloadWithData
    {};
    template <>
    struct Payload<ControlPacketType::SUBACK> final : public PayloadWithData
    {};
    template <>
    struct Payload<ControlPacketType::UNSUBACK> final : public PayloadWithData
    {};

    template <ControlPacketType type>
    struct PayloadSelector
    { typedef Payload<type> PayloadType; };

    template <>
    struct PayloadSelector<PUBLISH>
    { typedef PayloadWithData PayloadType; };
    template <>
    struct PayloadSelector<SUBACK>
    { typedef PayloadWithData PayloadType; };
    template <>
    struct PayloadSelector<UNSUBACK>
    { typedef PayloadWithData PayloadType; };

    // Those don't have any payload, let's simplify them
    template <>
    struct PayloadSelector<CONNACK>
    { typedef SerializablePayload PayloadType; };
    template <>
    struct PayloadSelector<PUBACK>
    { typedef SerializablePayload PayloadType; };
    template <>
    struct PayloadSelector<PUBREL>
    { typedef SerializablePayload PayloadType; };
    template <>
    struct PayloadSelector<PUBREC>
    { typedef SerializablePayload PayloadType; };
    template <>
    struct PayloadSelector<PUBCOMP>
    { typedef SerializablePayload PayloadType; };
    template <>
    struct PayloadSelector<DISCONNECT>
    { typedef SerializablePayload PayloadType; };
    template <>
    struct PayloadSelector<AUTH>
    { typedef SerializablePayload PayloadType; };

    template <ControlPacketType type>
    struct VHPropertyChooser;

    /** This is only valid for properties without an identifier */
    template <ControlPacketType type>
    struct VHPropertyChooser
    {
        typedef Properties VHProperty;
        typedef typename PayloadSelector<type>::PayloadType PayloadType;
    };

    /** The base for all control packet */
    struct ControlPacketSerializable : public ISerializable
    {
        virtual uint32_t computePacketSize(const bool includePayload = true) = 0;
    };
    /** The base for all control packet */
    struct ControlPacketSerializableImpl : public ControlPacketSerializable
    {
        /** The fixed header */
        FixedHeaderBase& header;
        /** The remaining length in bytes */
        VBInt remainingLength;
        /** The fixed variable header */
        FixedFieldGeneric& fixedVariableHeader;
        /** The variable header containing properties */
        Properties& props;
        /** The payload (if any required) */
        SerializablePayload& payload;

        uint32_t computePacketSize(const bool includePayload = true) override {
            uint32_t size = fixedVariableHeader.getSerializedSize() + props.getSerializedSize();
            if (includePayload)
            {
                size += payload.getSerializedSize();  // Add the payload size
                remainingLength = size;
            }
            remainingLength = (uint32_t)remainingLength - size;
            return (uint32_t)remainingLength;  // Return the size of the packet
        }

        uint32_t getSerializedSize() const override {
            return 1 + remainingLength.getSerializedSize() + static_cast<uint32_t>(remainingLength);
        }  //!< Get the size of the packet

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = header.typeandFlags;

            uint32_t offset = 1;
            offset += remainingLength.serialize(buffer + 1);
            offset +=
                fixedVariableHeader.serialize(buffer + offset);  // Serialize the variable header
            offset += props.serialize(buffer + offset);          // Serialize the properties

            offset += payload.serialize(buffer + offset);  // Serialize the payload
            return offset;                                 // Return the size of the packet
        }                                                  //!< Serialize the packet into the buffer
        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            header.typeandFlags = buffer[0];  // Deserialize the fixed header
            uint32_t offset = 1;
            if (offset == BadData)
                return BadData;  // Invalid data
            offset += remainingLength.deserialize(buffer + offset, bufferSize - offset);
            uint32_t expLength = (uint32_t)remainingLength;
            fixedVariableHeader.setRemainingLength(expLength);
            offset += fixedVariableHeader.deserialize(
                buffer + offset,
                bufferSize - offset);  // Deserialize the variable header
            offset += props.deserialize(buffer + offset,
                                        bufferSize - offset);  // Deserialize the properties
            payload.setExpectedPacketSize(computePacketSize(false));
            offset += payload.deserialize(buffer + offset,
                                          bufferSize - offset);  // Deserialize the payload
            return offset;                                       // Return the size of the packet
        }  //!< Deserialize the packet from the buffer

        bool checkImpl() const override {
            if (header.isValid() && remainingLength.checkImpl() &&
                fixedVariableHeader.checkImpl() && props.checkImpl() && payload.checkImpl())
            {
                return true;  // Check if the packet is valid
            }
            return false;  // Packet is not valid
        }                  //!< Check if the packet is valid

        ControlPacketSerializableImpl(FixedHeaderBase& header,
                                      FixedFieldGeneric& fixedVariableHeader, Properties& props,
                                      SerializablePayload& payload) :
            header(header),
            fixedVariableHeader(fixedVariableHeader),
            props(props),
            payload(payload) {}  //!< Constructor for the ControlPacketSerializableImpl class
        ~ControlPacketSerializableImpl() = default;
    };

    template <ControlPacketType type>
    struct ControlPacket : public ControlPacketSerializableImpl
    {
        typename ControlPacketCore<type>::FixedHeader header;
        typename FixedField<type> fixedVariableHeader;
        typename VHPropertyChooser<type>::VHProperty properties;
        typename PayloadSelector<type>::PayloadType payload;

        ControlPacket() :
            ControlPacketSerializableImpl(header, fixedVariableHeader, properties, payload) {
            payload.setFlags(fixedVariableHeader);
            fixedVariableHeader.setFlags(header.typeandFlags);
        }  //!< Default constructor for the ControlPacket class
    };

    struct PublishReplayPacket final : public ControlPacketSerializableImpl
    {
        FixedHeaderBase header;
        FixedField<PUBACK> fixedVariableHeader;
        typename VHPropertyChooser<PUBACK>::VHProperty properties;
        SerializablePayload payload;

        PublishReplayPacket() :
            ControlPacketSerializableImpl(header, fixedVariableHeader, properties, payload) {
        }  //!< Default constructor for the PublishReplayPacket class
        PublishReplayPacket(FixedHeaderBase& header, FixedFieldGeneric& fixedVariableHeader,
                            Properties& properties, SerializablePayload& payload) :
            ControlPacketSerializableImpl(header, fixedVariableHeader, properties, payload) {
        }  //!< Constructor for the PublishReplayPacket class
    };

    template <ControlPacketType type>
    struct PingPacket : public ControlPacketSerializable
    {
        typename ControlPacketCore<type>::FixedHeader header;

        PingPacket() {}
        uint32_t computePacketSize(const bool includePayload = true) override {
            return header.getSerializedSize();  //!< Get the size of the packet
        }                                       //!< Get the size of the packet

        uint32_t getSerializedSize() const override {
            return header.getSerializedSize();  //!< Get the size of the packet
        }                                       //!< Get the size of the packet
        uint32_t serialize(uint8_t* buffer) override {
            return header.serialize(buffer);  // Serialize the fixed header
        }                                     //!< Serialize the packet into the buffer
        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            return header.deserialize(buffer, bufferSize);  // Deserialize the fixed header
        }  //!< Deserialize the packet from the buffer

        bool checkImpl() const override {
            if (header.isValid())
            {
                return true;  // Check if the packet is valid
            }
            return false;  // Packet is not valid
        }                  //!< Check if the packet is valid
    };

    template <>
    struct ControlPacket<ControlPacketType::PINGREQ> final
        : public PingPacket<ControlPacketType::PINGREQ>
    {};
    template <>
    struct ControlPacket<ControlPacketType::PINGRESP> final
        : public PingPacket<ControlPacketType::PINGRESP>
    {};

    typedef ControlPacket<ControlPacketType::CONNECT> ConnectPacket;
    typedef ControlPacket<ControlPacketType::CONNACK> ConnAckPacket;
    typedef ControlPacket<ControlPacketType::PUBLISH> PublishPacket;
    typedef ControlPacket<ControlPacketType::PUBACK> PubAckPacket;
    typedef ControlPacket<ControlPacketType::PUBREC> PubRecPacket;
    typedef ControlPacket<ControlPacketType::PUBREL> PubRelPacket;
    typedef ControlPacket<ControlPacketType::PUBCOMP> PubCompPacket;
    typedef ControlPacket<ControlPacketType::SUBSCRIBE> SubscribePacket;
    typedef ControlPacket<ControlPacketType::SUBACK> SubAckPacket;
    typedef ControlPacket<ControlPacketType::UNSUBSCRIBE> UnsubscribePacket;
    typedef ControlPacket<ControlPacketType::UNSUBACK> UnsubAckPacket;
    typedef ControlPacket<ControlPacketType::DISCONNECT> DisconnectPacket;
    typedef ControlPacket<ControlPacketType::AUTH> AuthPacket;
    typedef ControlPacket<ControlPacketType::PINGREQ> PingReqPacket;
    typedef ControlPacket<ControlPacketType::PINGRESP> PingRespPacket;

    class PacketsBuilder
    {
    public:
        static ControlPacketSerializable* buildConnectPacket(
            const char* clientId = "", const char* username = "",
            const DynamicBinaryData* password = nullptr, bool cleanSession = true,
            uint16_t keepAlive = 60, WillMessage* willMessage = nullptr,
            const QoSDelivery willQoS = QoSDelivery::AtLeastOne, const bool willRetain = true,
            Properties* properties = nullptr);

        static ControlPacketSerializable* buildSubscribePacket(
            const uint16_t packetID = 0, const char* topic = "",
            const RetainHandling = RetainHandling::NoRetainedMessage,
            const bool withAutoFeedBack = false,
            const QoSDelivery maxAcceptedQos = QoSDelivery::ExactlyOne,
            const bool retainAsPublished = true, Properties* properties = nullptr);

        static ControlPacketSerializable* buildSubscribePacket(const uint16_t packetID = 0,
                                                               SubscribeTopic* topicList = nullptr,
                                                               Properties* properties = nullptr);

        static ControlPacketSerializable* buildUnsubscribePacket(
            const uint16_t packetID = 0, UnsubscribeTopic* topicList = nullptr,
            Properties* properties = nullptr);

        static ControlPacketSerializable* buildPublishPacket(
            const uint16_t packetID = 0, const char* topicName = "",
            const uint8_t* payload = nullptr, const uint32_t payloadSize = 0,
            const QoSDelivery qos = QoSDelivery::AtLeastOne, const bool retain = false,
            Properties* properties = nullptr);

        static ControlPacketSerializable* buildPublishReplayPacket(
            const uint16_t packetID, const QoSDelivery qos = QoSDelivery::AtLeastOne,
            const bool dup = false, Properties* properties = nullptr);

        static ControlPacketSerializable* buildPingPacket();
        static ControlPacketSerializable* buildDisconnectPacket(const uint16_t packetID = 0,
                                                                Properties* properties = nullptr);
        static ControlPacketSerializable* buildAuthPacket(
            const ReasonCode reasonCode, const uint16_t sessionExpiryInterval,
            const uint8_t serverKeepAlive, const DynamicStringView& uthenticationMethod,
            const DynamicBinaryDataView& authData, Properties* properties = nullptr);
        static ControlPacketSerializable* buildConAckPacket(const uint8_t sessionPresent,
                                                            const uint8_t reasonCode,
                                                            Properties* properties = nullptr);
        static ControlPacketSerializable* buildSubAckPacket(const uint16_t packetID,
                                                            const uint8_t reasonCode,
                                                            Properties* properties = nullptr);
        static ControlPacketSerializable* buildUnsubAckPacket(const uint16_t packetID,
                                                              const uint8_t reasonCode,
                                                              Properties* properties = nullptr);
        static ControlPacketSerializable* buildPubAckPacket(const uint16_t packetID,
                                                            const uint8_t reasonCode,
                                                            Properties* properties = nullptr);
        static ControlPacketSerializable* buildPubRecPacket(const uint16_t packetID,
                                                            const uint8_t reasonCode,
                                                            Properties* properties = nullptr);
        static ControlPacketSerializable* buildPubRelPacket(const uint16_t packetID,
                                                            const uint8_t reasonCode,
                                                            Properties* properties = nullptr);
        static ControlPacketSerializable* buildPubCompPacket(const uint16_t packetID,
                                                             const uint8_t reasonCode,
                                                             Properties* properties = nullptr);

        PacketsBuilder() = delete;                       //!< Delete the default constructor
        PacketsBuilder(const PacketsBuilder&) = delete;  //!< Delete the copy constructor
        PacketsBuilder(PacketsBuilder&&) = delete;       //!< Delete the move constructor
    };
}  // namespace MqttV5

#endif /* MQTTV5_MQTTV5PACKETS_HPP */
