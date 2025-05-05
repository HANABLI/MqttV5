#ifndef MQTTV5_ISERIALIZABLE_HPP
#define MQTTV5_ISERIALIZABLE_HPP

/**
 * @file MqttV5ISerializable.hpp
 * @brief This file contains the definition of the ISerializable interface.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */
#pragma once
#include <cstdint>
#include <cstddef>  // for size_t

namespace MqttV5
{
    /**
     * @brief This is the interface for all serializable objects.
     */
    class ISerializable
    {
    public:
        virtual ~ISerializable() = default;  //!< Destructor for the ISerializable class

        virtual uint32_t getSerializedSize() const = 0;   //!< Get the size of the serialized object
        virtual uint32_t serialize(uint8_t* buffer) = 0;  //!< Serialize the object into the buffer
        virtual uint32_t deserialize(
            const uint8_t* buffer,
            uint32_t bufferSize) = 0;        //!< Deserialize the object from the buffer
        virtual bool checkImpl() const = 0;  //!< Check the implementation of the object
    };

}  // namespace MqttV5

#endif /* MQTTV5_ISERIALIZABLE_HPP */