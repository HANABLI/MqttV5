/**
 * @file MqttV5Properties.cpp
 * @brief This file contains the implementation of the properties for MQTT V5.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 * @note This file is part of the MqttV5 library.
 */

#include "MqttV5/MqttV5Properties.hpp"
#include "MqttV5/MqttV5Types.hpp"

// #include "MqttV5Constants.hpp"

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
        PropertyCore* current = impl_->head;
        while (current != nullptr)
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
            offset += readSize;
        }

        return offset;
    }

    void Properties::addProperty(PropertyCore* property) {
        if (property == nullptr)
            throw std::invalid_argument("Property cannot be null.");
        property->next = nullptr;  // Sécurité : couper toute chaîne existante

        if (!impl_->head)
        {
            impl_->head = property;
            impl_->reference = property;
        } else
        {
            impl_->reference->next = property;
            impl_->reference = property;
        }
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

}  // namespace MqttV5