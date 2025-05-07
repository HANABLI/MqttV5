#ifndef MQTTV5_MQTTV5PROPERTIES_HPP
#define MQTTV5_MQTTV5PROPERTIES_HPP

/**
 * @file MqttV5Properties.hpp
 * @brief This file contains the definition of the properties for MQTT V5.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */

#pragma once

#include <cstddef>  // for uint32_t
#include <cstdint>
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <functional>
#include "MqttV5Constants.hpp"
#include "MqttV5ISerializable.hpp"
#include "MqttV5Types.hpp"

namespace MqttV5
{
    using namespace Common;

    struct PropertyCore : public ISerializable
    {
        PropertyId id;  //!< The ID of the property

        PropertyCore* next;  //!< Pointer to the next property in the list
        bool heapAllocated;  //!< Flag to indicate if the property is heap allocated

        uint32_t getSerializedSize() const override {
            return 0;
        }  //!< Get the size of the serialized object
        uint32_t serialize(uint8_t* buffer) override {
            return 0;
        }  //!< Serialize the object into the buffer
        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            return 0;
        }  //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            return true;
        }  //!< Check the implementation of the object

        virtual PropertyCore* clone() const = 0;

        PropertyCore(PropertyId id = BadProperty, const bool heap = false) :
            id(id), heapAllocated(heap) {}  //!< Constructor for the PropertyCore class
        virtual ~PropertyCore() {
            if (heapAllocated)  // Check if the property is heap allocated
                delete this;    //!< Delete the property if it is heap allocated
        }                       //!< Destructor for the PropertyCore class
    };

    struct PropertyCoreImpl : public PropertyCore
    {
        GenericTypeBase& value;  //!< value of the property
        uint32_t getSerializedSize() const override {
            return sizeof(PropertyId) +
                   value.typeSize();  //!< Get the size of the serialized object
        }                             //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = id;                                     // Copy the ID to the buffer
            value.swapNetworkOrder();                           // Swap the value to network order
            memcpy(buffer + 1, value.raw(), value.typeSize());  // Copy the value to the buffer
            value.swapNetworkOrder();                           // Swap the value back to host order
            return value.typeSize() + 1;                        // Return the size of the property
        }  //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if ((buffer[0] & 0x80) || buffer[0] != id)
                return BadData;  // Invalid property ID
            if (bufferSize < sizeof(PropertyId) + value.typeSize())
                return NotEnoughData;                           // Not enough data
            memcpy(value.raw(), buffer + 1, value.typeSize());  // Copy the value from the buffer
            value.swapNetworkOrder();                           // Swap the value to host order
            return value.typeSize() + 1;                        // Return the size of the property
        }  //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            if (!value.isValid())
                return false;  // No data
            if (value.typeSize() > 0xFFFF)
                return false;  // Invalid size
            return true;
        }  //!< Check the implementation of the object

        PropertyCore* clone() const override {
            return new PropertyCoreImpl(id, value, true);  // ou copier value proprement
        }

        PropertyCoreImpl(const PropertyId id, GenericTypeBase& v, const bool heap = false) :
            PropertyCore(id, heap), value(v) {}
        ~PropertyCoreImpl() = default;
    };

    template <typename T>
    struct Property final : public PropertyCoreImpl
    {
        GenericType<T> value;  //!< The value of the property

        // uint32_t getSerializedSize() const override {
        //     return sizeof(PropertyId) + sizeof(T);  //!< Get the size of the serialized object
        // }                                           //!< Get the size of the serialized object

        // uint32_t serialize(uint8_t* buffer) override {
        //     buffer[0] = id;                         // Copy the ID to the buffer
        //     memcpy(buffer + 1, &value, sizeof(T));  // Copy the value to the buffer
        //     return sizeof(PropertyId) + sizeof(T);  // Return the size of the property
        // }                                           //!< Serialize the object into the buffer

        // uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
        //     if ((buffer[0] & 0x80) || buffer[0] != id)
        //         return BadData;  // Invalid property ID
        //     if (bufferSize < sizeof(PropertyId) + sizeof(T))
        //         return NotEnoughData;               // Not enough data
        //     memcpy(&value, buffer + 1, sizeof(T));  // Copy the value from the buffer
        //     return sizeof(PropertyId) + sizeof(T);  // Return the size of the property
        // }                                           //!< Deserialize the object from the buffer

        // bool checkImpl() const override {
        //     if (!value.isValid())
        //         return false;  // No data
        //     if (sizeof(T) > 0xFFFF)
        //         return false;  // Invalid size
        //     return true;
        // }  //!< Check the implementation of the object

        PropertyCore* clone() const { return new Property((PropertyId)id, value.value, true); }

        Property(const PropertyId id = BadProperty, T val = 0, const bool heap = false) :
            value(val),
            PropertyCoreImpl(id, value, heap) {}  //!< Constructor for the Property class
        // Nouveau constructeur pour les objets temporaires
        // Property(const PropertyId id, T&& val, const bool heap = false) :
        //     PropertyCoreImpl(id, val, heap), value(std::move(val)) {}
        ~Property() = default;  //!< Destructor for the Property class
    };

    template <>
    struct Property<uint8_t> final : public PropertyCoreImpl
    {
        GenericType<uint8_t> value;  //!< The value of the property

        PropertyCore* clone() const { return new Property((PropertyId)id, value.value, true); }
        Property(const PropertyId id = BadProperty, uint8_t val = 0, const bool heap = false) :
            value(val),
            PropertyCoreImpl(id, value, heap) {}  //!< Constructor for the Property class

        ~Property() = default;  //!< Destructor for the Property class
    };

    template <>
    struct Property<DynamicString> final : public PropertyCore
    {
        DynamicString value;  //!< The value of the property

        uint32_t getSerializedSize() const override {
            return sizeof(PropertyId) +
                   value.getSerializedSize();  //!< Get the size of the serialized object
        }                                      //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = id;  // Copy the ID to the buffer
            uint32_t offset = 1;
            offset += value.serialize(buffer + offset);  // Serialize the value
            return offset;                               // Return the size of the property
        }                                                //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if ((buffer[0] & 0x80) || buffer[0] != id)
                return BadData;  // Invalid property ID
            if (bufferSize < sizeof(PropertyId) + value.getSerializedSize())
                return NotEnoughData;  // Not enough data
            uint32_t offset = 1;
            offset += value.deserialize(buffer + offset, bufferSize);  // Deserialize the value
            return offset;  // Return the size of the property
        }                   //!< Deserialize the object from the buffer
        PropertyCore* clone() const { return new Property((PropertyId)id, value, true); }
        // Constructor accepting a const reference
        Property(const PropertyId id = BadProperty, const DynamicString& val = DynamicString(),
                 bool heap = false) :
            PropertyCore(id, heap), value(val) {}

        // Constructor accepting an rvalue reference
        Property(const PropertyId id, DynamicString&& val, bool heap = false) :
            PropertyCore(id, heap), value(std::move(val)) {}
        ~Property() {
            if (heapAllocated)  // Check if the property is heap allocated
                delete this;    //!< Delete the property if it is heap allocated
        }                       //!< Destructor for the Property class
    };

    template <>
    struct Property<DynamicBinaryData> final : public PropertyCore
    {
        DynamicBinaryData value;  //!< The value of the property

        uint32_t getSerializedSize() const override {
            return sizeof(PropertyId) +
                   value.getSerializedSize();  //!< Get the size of the serialized object
        }                                      //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = id;  // Copy the ID to the buffer
            uint32_t offset = 1;
            offset += value.serialize(buffer + offset);  // Serialize the value
            return offset;                               // Return the size of the property
        }                                                //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if ((buffer[0] & 0x80) || buffer[0] != id)
                return BadData;  // Invalid property ID
            if (bufferSize < sizeof(PropertyId) + value.getSerializedSize())
                return NotEnoughData;  // Not enough data
            uint32_t offset = 1;
            offset += value.deserialize(buffer + offset, bufferSize);  // Deserialize the value
            return offset;  // Return the size of the property
        }                   //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            if (!value.checkImpl())
                return false;  // No data
            if (value.size > 0xFFFF)
                return false;  // Invalid size
            return true;
        }  //!< Check the implementation of the object
        PropertyCore* clone() const { return new Property((PropertyId)id, value, true); }
        Property(PropertyId id = BadProperty, const DynamicBinaryData& val = DynamicBinaryData(),
                 const bool heap = false) :
            PropertyCore(id, heap), value(val) {}  //!< Constructor for the Property class

        ~Property() {
            if (heapAllocated)  // Check if the property is heap allocated
                delete this;    //!< Delete the property if it is heap allocated
        }                       //!< Destructor for the Property class
    };

    template <>
    struct Property<DynamicStringPair> final : public PropertyCore
    {
        DynamicStringPair value;  //!< The value of the property

        uint32_t getSerializedSize() const override {
            return sizeof(PropertyId) +
                   value.getSerializedSize();  //!< Get the size of the serialized object
        }                                      //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = id;  // Copy the ID to the buffer
            uint32_t offset = 1;
            offset += value.serialize(buffer + offset);  // Serialize the value
            return offset;                               // Return the size of the property
        }                                                //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if ((buffer[0] & 0x80) || buffer[0] != id)
                return BadData;  // Invalid property ID
            if (bufferSize < sizeof(PropertyId) + value.getSerializedSize())
                return NotEnoughData;  // Not enough data
            uint32_t offset = 1;
            offset += value.deserialize(buffer + offset, bufferSize);  // Deserialize the value
            return offset;  // Return the size of the property
        }                   //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            if (!value.checkImpl())
                return false;  // No data
            if (value.getSerializedSize() > 0xFFFF)
                return false;  // Invalid size
            return true;
        }  //!< Check the implementation of the object
        PropertyCore* clone() const { return new Property((PropertyId)id, value, true); }
        Property(PropertyId id = BadProperty, const DynamicStringPair& val = DynamicStringPair(),
                 const bool heap = false) :
            PropertyCore(id, heap), value(val) {}  //!< Constructor for the Property class

        ~Property() {
            if (heapAllocated)  // Check if the property is heap allocated
                delete this;    //!< Delete the property if it is heap allocated
        }                       //!< Destructor for the Property class
    };

    template <>
    struct Property<DynamicStringView> final : public PropertyCore
    {
        DynamicStringView value;  //!< The value of the property

        uint32_t getSerializedSize() const override {
            return sizeof(PropertyId) +
                   value.getSerializedSize();  //!< Get the size of the serialized object
        }                                      //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = id;  // Copy the ID to the buffer
            uint32_t offset = 1;
            offset += value.serialize(buffer + offset);  // Serialize the value
            return offset;                               // Return the size of the property
        }                                                //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if ((buffer[0] & 0x80) || buffer[0] != id)
                return BadData;  // Invalid property ID
            if (bufferSize < sizeof(PropertyId) + value.getSerializedSize())
                return NotEnoughData;  // Not enough data
            uint32_t offset = 1;
            offset += value.deserialize(buffer + offset, bufferSize);  // Deserialize the value
            return offset;  // Return the size of the property
        }                   //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            if (!value.checkImpl())
                return false;  // No data
            if (value.size > 0xFFFF)
                return false;  // Invalid size
            return true;
        }  //!< Check the implementation of the object
        PropertyCore* clone() const { return new Property((PropertyId)id, value, true); }
        Property(PropertyId id = BadProperty, const DynamicStringView& val = DynamicStringView(),
                 const bool heap = false) :
            PropertyCore(id, heap), value(val) {}  //!< Constructor for the Property class

        ~Property() {
            if (heapAllocated)  // Check if the property is heap allocated
                delete this;    //!< Delete the property if it is heap allocated
        }                       //!< Destructor for the Property class
    };

    template <>
    struct Property<DynamicBinaryDataView> final : public PropertyCore
    {
        DynamicBinaryDataView value;  //!< The value of the property

        uint32_t getSerializedSize() const override {
            return sizeof(PropertyId) +
                   value.getSerializedSize();  //!< Get the size of the serialized object
        }                                      //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = id;  // Copy the ID to the buffer
            uint32_t offset = 1;
            offset += value.serialize(buffer + offset);  // Serialize the value
            return offset;                               // Return the size of the property
        }                                                //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if ((buffer[0] & 0x80) || buffer[0] != id)
                return BadData;  // Invalid property ID
            if (bufferSize < sizeof(PropertyId) + value.getSerializedSize())
                return NotEnoughData;  // Not enough data
            uint32_t offset = 1;
            offset += value.deserialize(buffer + offset, bufferSize);  // Deserialize the value
            return offset;  // Return the size of the property
        }                   //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            if (!value.checkImpl())
                return false;  // No data
            if (value.size > 0xFFFF)
                return false;  // Invalid size
            return true;
        }  //!< Check the implementation of the object
        PropertyCore* clone() const { return new Property((PropertyId)id, value, true); }
        // Constructor accepting a const reference
        Property(const PropertyId id = BadProperty,
                 const DynamicBinaryDataView& val = DynamicBinaryDataView(), bool heap = false) :
            PropertyCore(id, heap), value(val) {}

        // Constructor accepting an rvalue reference
        Property(const PropertyId id, DynamicBinaryDataView&& val, bool heap = false) :
            PropertyCore(id, heap), value(std::move(val)) {}
        ~Property() {
            if (heapAllocated)  // Check if the property is heap allocated
                delete this;    //!< Delete the property if it is heap allocated
        }                       //!< Destructor for the Property class
    };

    template <>
    struct Property<MappedVBInt> final : public PropertyCore
    {
        MappedVBInt value;  //!< The value of the property

        uint32_t getSerializedSize() const override {
            return sizeof(PropertyId) +
                   value.getSerializedSize();  //!< Get the size of the serialized object
        }                                      //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = id;  // Copy the ID to the buffer
            uint32_t offset = 1;
            offset += value.serialize(buffer + offset);  // Serialize the value
            return offset;                               // Return the size of the property
        }                                                //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if ((buffer[0] & 0x80) || buffer[0] != id)
                return BadData;  // Invalid property ID
            if (bufferSize < sizeof(PropertyId) + value.getSerializedSize())
                return NotEnoughData;  // Not enough data
            uint32_t offset = 1;
            offset += value.deserialize(buffer + offset, bufferSize);  // Deserialize the value
            return offset;  // Return the size of the property
        }                   //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            if (!value.checkImpl())
                return false;  // No data
            if (value.size > 0xFFFF)
                return false;  // Invalid size
            return true;
        }  //!< Check the implementation of the object
        PropertyCore* clone() const { return new Property((PropertyId)id, value, true); }
        Property(PropertyId id = BadProperty, const MappedVBInt& val = MappedVBInt(),
                 const bool heap = false) :
            PropertyCore(id, heap), value(val) {}  //!< Constructor for the Property class

        ~Property() {
            if (heapAllocated)  // Check if the property is heap allocated
                delete this;    //!< Delete the property if it is heap allocated
        }                       //!< Destructor for the Property class
    };

    template <>
    struct Property<DynamicStringPairView> final : public PropertyCore
    {
        DynamicStringPairView value;  //!< The value of the property

        uint32_t getSerializedSize() const override {
            return sizeof(PropertyId) +
                   value.getSerializedSize();  //!< Get the size of the serialized object
        }                                      //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = id;  // Copy the ID to the buffer
            uint32_t offset = 1;
            offset += value.serialize(buffer + offset);  // Serialize the value
            return offset;                               // Return the size of the property
        }                                                //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if ((buffer[0] & 0x80) || buffer[0] != id)
                return BadData;  // Invalid property ID
            if (bufferSize < sizeof(PropertyId) + value.getSerializedSize())
                return NotEnoughData;  // Not enough data
            uint32_t offset = 1;
            offset += value.deserialize(buffer + offset, bufferSize);  // Deserialize the value
            return offset;  // Return the size of the property
        }                   //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            if (!value.checkImpl())
                return false;  // No data
            if (value.getSerializedSize() > 0xFFFF)
                return false;  // Invalid size
            return true;
        }  //!< Check the implementation of the object

        PropertyCore* clone() const { return new Property((PropertyId)id, value, true); }
        Property(PropertyId id = BadProperty,
                 const DynamicStringPairView& val = DynamicStringPairView(),
                 const bool heap = false) :
            PropertyCore(id, heap), value(val) {}  //!< Constructor for the Property class

        ~Property() {
            if (heapAllocated)  // Check if the property is heap allocated
                delete this;    //!< Delete the property if it is heap allocated
        }                       //!< Destructor for the Property class
    };

    template <>
    struct Property<VBInt> final : public PropertyCore
    {
        VBInt value;  //!< The value of the property

        uint32_t getSerializedSize() const override {
            return sizeof(PropertyId) +
                   value.getSerializedSize();  //!< Get the size of the serialized object
        }                                      //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override {
            buffer[0] = id;  // Copy the ID to the buffer
            uint32_t offset = 1;
            offset += value.serialize(buffer + offset);  // Serialize the value
            return offset;                               // Return the size of the property
        }                                                //!< Serialize the object into the buffer

        uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
            if ((buffer[0] & 0x80) || buffer[0] != id)
                return BadData;  // Invalid property ID
            if (bufferSize < sizeof(PropertyId) + value.getSerializedSize())
                return NotEnoughData;  // Not enough data
            uint32_t offset = 1;
            offset += value.deserialize(buffer + offset, bufferSize);  // Deserialize the value
            return offset;  // Return the size of the property
        }                   //!< Deserialize the object from the buffer

        bool checkImpl() const override {
            if (!value.checkImpl())
                return false;  // No data
            if (value.size > 0xFFFF)
                return false;  // Invalid size
            return true;
        }  //!< Check the implementation of the object
        PropertyCore* clone() const { return new Property((PropertyId)id, value, true); }
        Property(PropertyId id = BadProperty, const VBInt& val = VBInt(), const bool heap = false) :
            PropertyCore(id, heap), value(val) {}  //!< Constructor for the Property class

        ~Property() {
            if (heapAllocated)  // Check if the property is heap allocated
                delete this;    //!< Delete the property if it is heap allocated
        }                       //!< Destructor for the Property class
    };

    template <PropertyId id, typename T>
    struct PropertyFactory
    {
        static const PropertyId type = id;
        static PropertyCore* create() {
            T defaultValue = T();
            return new Property<T>(type, defaultValue, false);
        }  //!< Create a new property with the given ID and default value.
        static PropertyCore* create(T value) {
            return new Property<T>(type, value, false);
        }  //!< Create a new property with the given ID and value.
    };

    typedef PropertyFactory<PropertyId::PayloadFormatIndicator, uint8_t>
        PayloadFormatIndicator_prop;  //!< Factory for byte properties
    typedef PropertyFactory<PropertyId::MessageExpiryInterval, uint32_t>
        MessageExpiryInterval_prop;  //!< Factory for uint32_t properties
    typedef PropertyFactory<PropertyId::ContentType, DynamicString>
        ContentType_prop;  //!< Factory for string properties
    typedef PropertyFactory<PropertyId::ResponseTopic, DynamicString>
        ResponseTopic_prop;  //!< Factory for string properties
    typedef PropertyFactory<PropertyId::CorrelationData, DynamicBinaryData>
        CorrelationData_prop;  //!< Factory for binary properties
    typedef PropertyFactory<PropertyId::UserProperty, DynamicStringPair>
        UserProperty_prop;  //!< Factory for string pair properties
    typedef PropertyFactory<PropertyId::SubscriptionID, uint32_t>
        SubscriptionIdentifier_prop;  //!< Factory for uint32_t properties
    typedef PropertyFactory<PropertyId::SessionExpiryInterval, uint32_t>
        SessionExpiryInterval_prop;  //!< Factory for uint32_t properties
    typedef PropertyFactory<PropertyId::ServerKeepAlive, uint16_t>
        ServerKeepAlive_prop;  //!< Factory for uint16_t properties
    typedef PropertyFactory<PropertyId::ResponseInfo, DynamicString>
        ResponseInformation_prop;  //!< Factory for string properties
    typedef PropertyFactory<PropertyId::ServerReference, DynamicString>
        ServerReference_prop;  //!< Factory for string properties
    typedef PropertyFactory<PropertyId::AuthenticationMethod, DynamicString>
        AuthenticationMethod_prop;  //!< Factory for string properties
    typedef PropertyFactory<PropertyId::AuthenticationData, DynamicBinaryData>
        AuthenticationData_prop;  //!< Factory for binary properties
    typedef PropertyFactory<PropertyId::ReasonString, DynamicString>
        ReasonString_prop;  //!< Factory for string properties
    typedef PropertyFactory<PropertyId::TopicAlias, uint16_t>
        TopicAlias_prop;  //!< Factory for uint16_t properties
    typedef PropertyFactory<PropertyId::TopicAliasMax, uint16_t>
        TopicAliasMaximum_prop;  //!< Factory for uint16_t properties
    typedef PropertyFactory<PropertyId::PacketSizeMax, uint32_t>
        MaximumPacketSize_prop;  //!< Factory for uint32_t properties
    typedef PropertyFactory<PropertyId::QoSMax, uint8_t>
        MaximumQoS_prop;  //!< Factory for byte properties
    typedef PropertyFactory<PropertyId::RetainAvailable, uint8_t>
        RetainAvailable_prop;  //!< Factory for byte properties
    typedef PropertyFactory<PropertyId::WildcardSubAvailable, uint8_t>
        WildcardSubscriptionAvailable_prop;  //!< Factory for byte properties
    typedef PropertyFactory<PropertyId::SubscriptionIDAvailable, uint8_t>
        SubscriptionIdentifierAvailable_prop;  //!< Factory for byte properties
    typedef PropertyFactory<PropertyId::SharedSubscriptionAvailable, uint8_t>
        SharedSubscriptionAvailable_prop;  //!< Factory for byte properties
    typedef PropertyFactory<PropertyId::ReceiveMax, uint16_t>
        ReceiveMaximum_prop;  //!< Factory for uint16_t properties
    typedef PropertyFactory<PropertyId::TopicAlias, uint16_t>
        TopicAlias_prop;  //!< Factory for uint16_t properties
    typedef PropertyFactory<PropertyId::WillDelayInterval, uint32_t>
        WillDelayInterval_prop;  //!< Factory for uint32_t properties
    typedef PropertyFactory<PropertyId::RequestResponseInfo, uint8_t>
        RequestResponseInformation_prop;  //!< Factory for byte properties
    typedef PropertyFactory<PropertyId::RequestProblemInfo, uint8_t>
        RequestProblemInformation_prop;  //!< Factory for byte properties
    typedef PropertyFactory<PropertyId::AssignedClientID, DynamicString>
        AssignedClientIdentifier_prop;  //!< Factory for string properties
    typedef PropertyFactory<PropertyId::BadProperty, uint8_t>
        BadProperty_prop;  //!< Factory for byte properties

    struct DeserializationRegistry
    {
        using DeserializerFunc = std::function<PropertyCore*()>;

        static DeserializationRegistry& getInstance() {
            static DeserializationRegistry instance;
            return instance;
        }

        void registerDeserializer(PropertyId id, DeserializerFunc func) {
            std::lock_guard<std::mutex> lock(mutex_);
            registry_[id] = func;
        }

        PropertyCore* deserialize(PropertyId id) const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = registry_.find(id);
            if (it != registry_.end())
            { return it->second(); }
            return nullptr;
        }

        void clearRegistery() {
            std::lock_guard<std::mutex> lock(mutex_);
            registry_.clear();
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<PropertyId, DeserializerFunc> registry_;
    };

    template <typename T>
    struct RegisterProperty
    {
        RegisterProperty() {
            DeserializationRegistry::getInstance().registerDeserializer(
                T::type, []() { return T::create(); });
        }
    };

    static inline void registerAllProperties() {
        static bool isRegistered = false;
        if (!isRegistered)
        {
            RegisterProperty<PayloadFormatIndicator_prop>();
            RegisterProperty<MessageExpiryInterval_prop>();
            RegisterProperty<ContentType_prop>();
            RegisterProperty<ResponseTopic_prop>();
            RegisterProperty<CorrelationData_prop>();
            RegisterProperty<UserProperty_prop>();
            RegisterProperty<SubscriptionIdentifier_prop>();
            RegisterProperty<SessionExpiryInterval_prop>();
            RegisterProperty<ServerKeepAlive_prop>();
            RegisterProperty<ReasonString_prop>();
            RegisterProperty<TopicAlias_prop>();
            RegisterProperty<MaximumPacketSize_prop>();
            RegisterProperty<MaximumQoS_prop>();
            RegisterProperty<RetainAvailable_prop>();
            RegisterProperty<WildcardSubscriptionAvailable_prop>();
            RegisterProperty<SubscriptionIdentifierAvailable_prop>();
            RegisterProperty<SharedSubscriptionAvailable_prop>();
            RegisterProperty<RequestResponseInformation_prop>();
            RegisterProperty<RequestProblemInformation_prop>();
            RegisterProperty<AuthenticationMethod_prop>();
            RegisterProperty<AuthenticationData_prop>();
            RegisterProperty<AssignedClientIdentifier_prop>();
            RegisterProperty<ServerReference_prop>();
            RegisterProperty<ResponseInformation_prop>();
            RegisterProperty<WillDelayInterval_prop>();
            RegisterProperty<ReceiveMaximum_prop>();
            RegisterProperty<TopicAliasMaximum_prop>();
            isRegistered = true;
        }
    }

    class Properties final : public ISerializable
    {
    public:
        uint32_t getSerializedSize() const override;  //!< Get the size of the serialized object

        uint32_t serialize(uint8_t* buffer) override;  //!< Serialize the object into the buffer

        uint32_t deserialize(
            const uint8_t* buffer,
            uint32_t bufferSize) override;  //!< Deserialize the object from the buffer

        bool checkImpl() const override;  //!< Check the implementation of the properties class

        /**
         * use DeserializationRegistry to deserialize all properties
         */
        void initialize();  //!< Initialize the properties class

        /**
         * clear the properties class
         */
        void clear();  //!< Clear the properties class

        /**
         * ajoute une propriété à la neude de la liste des propriétés
         * @param property la propriété à ajouter
         * @return true si la propriété a été ajoutée, false sinon
         */
        void addProperty(PropertyCore* property);  //!< Add a property to the properties class

        /**
         * get à property by id and index
         * @param id l'id de la propriété à récupérer
         * @param index l'index de la propriété à récupérer
         * @return la propriété récupérée
         * @note if index is 0, the first property with the given id is returned
         */
        PropertyCore* getProperty(PropertyId id) const;  //!< Get a property by ID

        /**
         * get a property by index
         * @param index l'index de la propriété à récupérer
         * @return la propriété récupérée
         */
        PropertyCore* getPropertyAt(uint32_t index) const;  //!< Get a property by index

        /**
         * get the number of properties in the properties class
         * @return whether the other properties class has captured or not.
         */
        bool captureProperties(
            const Properties& other);  //!< Capture properties from another object
    public:
        Properties();                             //!< Constructor for the Properties class
        Properties(const Properties& other);      //!< Copy constructor for the Properties class
        Properties(Properties&& other) noexcept;  //!< Move constructor for the Properties class
        Properties& operator=(const Properties& other);      //!< Copy assignment operator
        Properties& operator=(Properties&& other) noexcept;  //!< Move assignment operator
        ~Properties() = default;  //!< Destructor for the Properties class
    private:
        /* data */

        struct Impl;

        std::shared_ptr<struct Impl>
            impl_;  //!< Pointer to the implementation of the properties class
    };

}  // namespace MqttV5
#endif /* MQTTV5_MQTTPROPERTIES_HPP */
