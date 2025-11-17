/**
 * @file MqttV5Properties.cpp
 * @brief This file contains the implementation of the properties for MQTT V5.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 * @note This file is part of the MqttV5 library.
 */

#include "MqttV5/MqttV5Properties.hpp"

// #include "MqttV5Constants.hpp"
namespace MqttV5
{
    /** The allowed properties for each control packet type.
    This is used externally to allow generic code to be written */
    template <PropertyId type>
    struct ExpectedProperty
    {
        enum
        {
            AllowedMask = 0
        };
    };
    template <>
    struct ExpectedProperty<PayloadFormatIndicator>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)PUBLISH) | 1
        };
    };
    template <>
    struct ExpectedProperty<MessageExpiryInterval>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)PUBLISH) | 1
        };
    };
    template <>
    struct ExpectedProperty<ContentType>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)PUBLISH) | 1
        };
    };
    template <>
    struct ExpectedProperty<ResponseTopic>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)PUBLISH) | 1
        };
    };
    template <>
    struct ExpectedProperty<CorrelationData>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)PUBLISH) | 1
        };
    };
    template <>
    struct ExpectedProperty<TopicAlias>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)PUBLISH)
        };
    };
    template <>
    struct ExpectedProperty<WillDelayInterval>
    {
        enum
        {
            AllowedMask = 1
        };
    };
    template <>
    struct ExpectedProperty<SubscriptionID>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)PUBLISH) | (1 << (uint8_t)SUBSCRIBE)
        };
    };
    template <>
    struct ExpectedProperty<SessionExpiryInterval>
    {
        enum
        {
            AllowedMask =
                (1 << (uint8_t)CONNECT) | (1 << (uint8_t)CONNACK) | (1 << (uint8_t)DISCONNECT)
        };
    };
    template <>
    struct ExpectedProperty<AuthenticationMethod>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNECT) | (1 << (uint8_t)CONNACK) | (1 << (uint8_t)AUTH)
        };
    };
    template <>
    struct ExpectedProperty<AuthenticationData>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNECT) | (1 << (uint8_t)CONNACK) | (1 << (uint8_t)AUTH)
        };
    };
    template <>
    struct ExpectedProperty<ReceiveMax>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNECT) | (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<TopicAliasMax>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNECT) | (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<PacketSizeMax>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNECT) | (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<RequestProblemInfo>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNECT)
        };
    };
    template <>
    struct ExpectedProperty<RequestResponseInfo>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNECT)
        };
    };
    template <>
    struct ExpectedProperty<AssignedClientID>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<ServerKeepAlive>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<QoSMax>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<RetainAvailable>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<WildcardSubAvailable>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<SubscriptionIDAvailable>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<SharedSubscriptionAvailable>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<ResponseInfo>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNACK)
        };
    };
    template <>
    struct ExpectedProperty<ServerReference>
    {
        enum
        {
            AllowedMask = (1 << (uint8_t)CONNACK) | (1 << (uint8_t)DISCONNECT)
        };
    };
    template <>
    struct ExpectedProperty<ReasonString>
    {
        enum
        {
            AllowedMask =
                (1 << (uint8_t)CONNACK) | (1 << (uint8_t)PUBACK) | (1 << (uint8_t)PUBREC) |
                (1 << (uint8_t)PUBREL) | (1 << (uint8_t)PUBCOMP) | (1 << (uint8_t)SUBACK) |
                (1 << (uint8_t)UNSUBACK) | (1 << (uint8_t)DISCONNECT) | (1 << (uint8_t)AUTH)
        };
    };
    template <>
    struct ExpectedProperty<UserProperty>
    {
        enum
        {
            AllowedMask = 0xFFFF
        };
    };
    /** Check if the given property is allowed in the given control packet type in O(1) */
    static inline bool isAllowedProperty(
        const PropertyId type,
        const ControlPacketType
            ctype) {  // This takes 82 bytes of program memory by allowing O(1) in property
                      // validity, compared to O(N) search and duplicated code everywhere.
        static uint16_t allowedProperties[MaxUsedPropertyType] = {
            ExpectedProperty<(PropertyId)1>::AllowedMask,
            ExpectedProperty<(PropertyId)2>::AllowedMask,
            ExpectedProperty<(PropertyId)3>::AllowedMask,
            ExpectedProperty<(PropertyId)4>::AllowedMask,
            ExpectedProperty<(PropertyId)5>::AllowedMask,
            ExpectedProperty<(PropertyId)6>::AllowedMask,
            ExpectedProperty<(PropertyId)7>::AllowedMask,
            ExpectedProperty<(PropertyId)8>::AllowedMask,
            ExpectedProperty<(PropertyId)9>::AllowedMask,
            ExpectedProperty<(PropertyId)10>::AllowedMask,
            ExpectedProperty<(PropertyId)11>::AllowedMask,
            ExpectedProperty<(PropertyId)12>::AllowedMask,
            ExpectedProperty<(PropertyId)13>::AllowedMask,
            ExpectedProperty<(PropertyId)14>::AllowedMask,
            ExpectedProperty<(PropertyId)15>::AllowedMask,
            ExpectedProperty<(PropertyId)16>::AllowedMask,
            ExpectedProperty<(PropertyId)17>::AllowedMask,
            ExpectedProperty<(PropertyId)18>::AllowedMask,
            ExpectedProperty<(PropertyId)19>::AllowedMask,
            ExpectedProperty<(PropertyId)20>::AllowedMask,
            ExpectedProperty<(PropertyId)21>::AllowedMask,
            ExpectedProperty<(PropertyId)22>::AllowedMask,
            ExpectedProperty<(PropertyId)23>::AllowedMask,
            ExpectedProperty<(PropertyId)24>::AllowedMask,
            ExpectedProperty<(PropertyId)25>::AllowedMask,
            ExpectedProperty<(PropertyId)26>::AllowedMask,
            ExpectedProperty<(PropertyId)27>::AllowedMask,
            ExpectedProperty<(PropertyId)28>::AllowedMask,
            ExpectedProperty<(PropertyId)29>::AllowedMask,
            ExpectedProperty<(PropertyId)30>::AllowedMask,
            ExpectedProperty<(PropertyId)31>::AllowedMask,
            ExpectedProperty<(PropertyId)32>::AllowedMask,
            ExpectedProperty<(PropertyId)33>::AllowedMask,
            ExpectedProperty<(PropertyId)34>::AllowedMask,
            ExpectedProperty<(PropertyId)35>::AllowedMask,
            ExpectedProperty<(PropertyId)36>::AllowedMask,
            ExpectedProperty<(PropertyId)37>::AllowedMask,
            ExpectedProperty<(PropertyId)38>::AllowedMask,
            ExpectedProperty<(PropertyId)39>::AllowedMask,
            ExpectedProperty<(PropertyId)40>::AllowedMask,
            ExpectedProperty<(PropertyId)41>::AllowedMask,
            ExpectedProperty<(PropertyId)42>::AllowedMask,
        };
        if (!type || type >= MaxUsedPropertyType)
            return 0;
        return (allowedProperties[(int)type - 1] & (1 << (uint8_t)ctype)) > 0;
    }
}  // namespace MqttV5
namespace MqttV5
{
    struct Properties::Impl
    {
        VBInt length;
        PropertyCore* head;
        PropertyCore* reference;  //!< Maximum size of the properties vector
        Impl() : head(nullptr), reference(nullptr) {}
        Impl(Impl&& other) noexcept :
            length(other.length), head(other.head), reference(other.reference) {
            other.head = nullptr;
            other.reference = nullptr;
        }  //!< Move constructor for the Impl class
        ~Impl() {
            PropertyCore* current = head;
            while (current != nullptr)
            {
                PropertyCore* next = current->next;
                delete current;  // Delete the property
                current = next;  // Move to the next property
            }
        }  //!< Destructor for the Impl class

        /* data */
    };

    Properties::Properties() : impl_(new Impl) {}  //!< Constructor for the Properties class

    Properties::Properties(const Properties& other) : impl_(new Impl) {
        impl_->length = other.impl_->length;
        impl_->head = nullptr;
        impl_->reference = nullptr;
        PropertyCore* current = other.impl_->head;
        while (current != nullptr)
        {
            PropertyCore* newProperty = current->clone();
            if (impl_->head == nullptr)
            {
                impl_->head = newProperty;
            } else
            {
                PropertyCore* temp = impl_->head;
                while (temp->next != nullptr)
                { temp = temp->next; }
                temp->next = newProperty;
            }
            current = current->next;
        }
    }  //!< Copy constructor for the Properties class

    Properties::Properties(Properties&& other) noexcept :
        impl_(std::move(other.impl_)) {  // Transfer ownership of the implementation
        other.impl_ = nullptr;           // Ensure the moved-from object is in a valid state
    }

    Properties& Properties::operator=(const Properties& other) {
        if (this != &other)
        {
            clear();  // Clear the current properties
            impl_->length = other.impl_->length;
            PropertyCore* current = other.impl_->head;
            while (current != nullptr)
            {
                PropertyCore* newProperty = current->clone();
                if (impl_->head == nullptr)
                {
                    impl_->head = newProperty;
                } else
                {
                    PropertyCore* temp = impl_->head;
                    while (temp->next != nullptr)
                    { temp = temp->next; }
                    temp->next = newProperty;
                }
                current = current->next;
            }
        }
        return *this;  //!< Return the properties class
    }                  //!< Assignment operator for the Properties class
    uint32_t Properties::getSerializedSize() const {
        uint32_t size = 0;
        PropertyCore* current = impl_->head;
        while (current != nullptr)
        {
            size += current->getSerializedSize();  // Get the size of the serialized object
            current = current->next;               // Move to the next property
        }
        return size;  //!< Return the size of the properties class
    }                 //!< Get the size of the serialized object

    uint32_t Properties::serialize(uint8_t* buffer) {
        if (!buffer)
            throw std::invalid_argument("Buffer cannot be null.");
        uint32_t offset = 0;
        offset += impl_->length.serialize(buffer);
        PropertyCore* current = impl_->head;
        while (current)
        {
            offset += current->serialize(buffer + offset);  // Serialize the object into the buffer
            current = current->next;                        // Move to the next property
        }
        return offset;  //!< Return the size of the properties class
    }                   //!< Serialize the object into the buffer

    uint32_t Properties::deserialize(const uint8_t* buffer, uint32_t bufferSize) {
        if (!buffer)
            throw std::invalid_argument("buffer is null");

        clear();
        registerAllProperties();  // s’assurer que tous les types sont enregistrés

        uint32_t offset = 0;
        offset += impl_->length.deserialize(buffer, bufferSize);
        if ((uint32_t)impl_->length > bufferSize - impl_->length.getSerializedSize())
            return NotEnoughData;
        auto propSize = (uint32_t)impl_->length;
        while (propSize)
        {
            PropertyId id = static_cast<PropertyId>(buffer[offset]);

            auto* prop = DeserializationRegistry::getInstance().deserialize(id);
            if (!prop)
                throw std::runtime_error("Unknown PropertyId during deserialization");

            uint32_t readSize = prop->deserialize(buffer + offset, bufferSize - offset);
            if (readSize == 0)
            {
                delete prop;
                throw std::runtime_error("Deserialization failed for property");
            }

            addProperty(prop);
            propSize -= readSize;
            offset += readSize;
        }

        return offset;
    }

    void Properties::addProperty(PropertyCore* property) {
        if (property == nullptr)
            throw std::invalid_argument("Property cannot be null.");
        property->next = nullptr;  // Sécurité : couper toute chaîne existante
        VBInt l((uint32_t)impl_->length + property->getSerializedSize());

        if (!impl_->head)
        {
            impl_->head = property;
            impl_->reference = property;
        } else
        {
            impl_->reference->next = property;
            impl_->reference = property;
        }
        impl_->length = l;
    }  //!< Add a property to the properties class

    PropertyCore* Properties::getProperty(PropertyId id) const {
        PropertyCore* current = impl_->head;
        while (current != nullptr)
        {
            if (current->id == id)
                return current;       // Return the property if found
            current = current->next;  // Move to the next property
        }
        return nullptr;  //!< Return null if the property is not found
    }                    //!< Get a property from the properties class

    PropertyCore* Properties::getPropertyAt(uint32_t index) const {
        if (index >= getSerializedSize())
            return nullptr;  // Index out of range
        PropertyCore* current = impl_->head;
        for (uint32_t i = 0; i < index; ++i)
        {
            current = current->next;  // Move to the next property
        }
        return current;  //!< Return the property at the specified index
    }                    //!< Get a property from the properties class

    bool Properties::captureProperties(const Properties& other) {
        if (this == &other)
            return false;  // Avoid self-assignment
        clear();           // Clear the current properties
        impl_->length = other.impl_->length;
        PropertyCore* current = other.impl_->head;
        while (current != nullptr)
        {
            PropertyCore* newProperty = current->clone();
            if (!impl_->head)
            {
                impl_->head = newProperty;
                impl_->reference = newProperty;
            } else
            {
                impl_->reference->next = newProperty;
                impl_->reference = newProperty;
            }

            current = current->next;
        }
        return true;  //!< Return true if the properties were captured successfully
    }                 //!< Capture properties from another object
    bool Properties::checkImpl() const {
        PropertyCore* current = impl_->head;
        while (current != nullptr)
        {
            if (!current->checkImpl())
                return false;
            current = current->next;
        }
        return true;
    }
    void Properties::initialize() { registerAllProperties(); }  //!< Initialize the properties class
    void Properties::clear() {
        if (impl_->head != nullptr)
        {
            PropertyCore* current = impl_->head;
            while (current != nullptr)
            {
                PropertyCore* next = current->next;
                delete current;
                current = next;
            }
            impl_->head = nullptr;
            impl_->reference = nullptr;
            impl_->length = 0;
        }
    }  //!< Clear the properties class

    bool Properties::checkPropertiesFor(const ControlPacketType type) const {
        if (!checkImpl())
            return false;
        PropertyCore* u = impl_->head;
        while (u)
        {
            if (!isAllowedProperty((PropertyId)u->id, type))
                return false;
            u = u->next;
        }
        return true;
    }
}  // namespace MqttV5
