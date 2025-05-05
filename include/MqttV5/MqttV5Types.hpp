#ifndef MQTTV5_MQTTV5TYPES_HPP
#define MQTTV5_MQTTV5TYPES_HPP
/**
 * @file MqttV5Types.hpp
 * @brief This file contains de definition of structures types for
 *        encoding and decoding MQTT messages.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <array>
#include <vector>
#include <string>
#include <string_view>
#include "MqttV5Constants.hpp"
#include "MqttV5ISerializable.hpp"

namespace
{
    /**
     * @brief function that should never be omitted, it is used to check the endianess of the
     * system.
     */
    inline uint8_t BigEndian(uint8_t value) {
        return value;
    }  //!< Big endian conversion for uint8_t
    /**
     * * @brief Converts a 16-bit integer to big endian format.
     * * @param value The integer to convert.
     */
    inline uint16_t BigEndian(uint16_t value) {
        return (value << 8) | (value >> 8);
    }  //!< Big endian conversion for uint16_t
    /**
     * * @brief Converts a 32-bit integer to big endian format.
     * * @param value The integer to convert.
     */
    inline uint32_t BigEndian(uint32_t value) {
        constexpr unsigned long mask = 0x01020304UL;
        if (4 == (unsigned char&)mask)  // Check if the system is little endian
            return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) |
                   ((value & 0x00FF0000) >> 8) |
                   (value >> 24);       //!< Little endian conversion for uint32_t
        if (1 == (unsigned char&)mask)  // Check if the system is big endian
            return value;               //!< No conversion needed for big endian
        return 0;                       //!< Unknown endianess
    }
}  // namespace

namespace MqttV5
{
    namespace Common
    {
        /** This is the standard error code while reading an invalid value from source */
        enum LocalError
        {
            BadData = 0xFFFFFFFF,        //!< Malformed data
            NotEnoughData = 0xFFFFFFFE,  //!< Not enough data
            Shortcut = 0xFFFFFFFD,       //!< Serialization shortcut used (not necessarly an error)

            MinErrorCode = 0xFFFFFFFD,
        };

        struct GenericTypeBase
        {
            virtual uint32_t typeSize() const = 0;  //!< Get the size of the type

            virtual void swapNetworkOrder() const = 0;  //!< Swap the type to network order

            virtual void* raw() = 0;  //!< Get the raw data of the type

            bool isValid() const { return true; }
        };

        template <typename T>
        struct GenericType : public GenericTypeBase
        {
            T value;  //!< The value of the type

            GenericType(T value = 0) : value(value) {}  //!< Constructor for the GenericType class
            operator T&() { return value; }
            GenericType& operator=(const T v) {
                value = v;
                return *this;
            }
            uint32_t typeSize() const override { return sizeof(T); }  //!< Get the size of the type

            void swapNetworkOrder() const override {
                // Swap the type to network order
                const_cast<T&>(value) = BigEndian(value);
            }  //!< Swap the type to network order

            void* raw() override { return &value; }  //!< Get the raw data of the type
        };

        /**
         * @brief A cross-platform bitfield class that should be used in union like this:
         *
         * @code
         *           union
         *           {
         *               T whatever;
         *               BitField<T, 0, 1> firstBit;
         *               BitField<T, 7, 1> lastBit;
         *               BitField<T, 2, 2> someBits;
         *           };
         */
        template <typename T, int Offset, int Bits>
        struct BitField
        {
            T value;

            static_assert(Offset + Bits <= (int)sizeof(T) * 8,
                          "Member exceeds bitfield boundaries");
            static_assert(Bits < (int)sizeof(T) * 8, "Can't fill entire bitfield with one member");

            static constexpr T Maximum = (T(1) << Bits) - 1;
            static constexpr T Mask = Maximum << Offset;

            inline operator T() const { return (value >> Offset) & Maximum; }
            inline BitField& operator=(T v) {
                value = (value & ~Mask) | ((v & Maximum) << Offset);
                return *this;
            }
        };

        /**
         * @brief Interface for serializable objects.
         */

        struct VBInt : public ISerializable
        {
            enum
            {
                MaxSizeOn1Byte = 0x7F,         //!< Maximum size of a varint on one byte
                MaxSizeOn2Bytes = 0x3FFF,      //!< Maximum size of a varint on two bytes
                MaxSizeOn3Bytes = 0x1FFFFF,    //!< Maximum size of a varint on three bytes
                MaxSizeOn4Bytes = 0x0FFFFFFF,  //!< Maximum size of a varint on four bytes
            };

            union {
                std::array<uint8_t, 4> raw;  //!< The raw data of the varint
                uint32_t word;               //!< The value of the varint
            };

            uint16_t size;  //!< The size of the varint in bytes
            /**
             * * @brief assignment operator for the varint.
             * * @param other The varint to assign.
             * * @return A reference to the varint.
             */
            VBInt& operator=(const VBInt& other) {
                word = other.word;
                size = other.size;
                return *this;
            }

            /**
             * * @brief Converts a uint32_t to a varint.
             * * @param other The uint32_t to convert.
             */
            VBInt& operator=(uint32_t other) {
                word = other;
                size = 0;

                if (other > 0x0FFFFFFF)
                {
                    size = 0;  // valeur invalide selon MQTT v5
                    return *this;
                }

                do
                {
                    uint8_t byte = other & 0x7F;
                    other >>= 7;
                    if (other > 0)
                        byte |= 0x80;  // bit de continuation
                    raw[size++] = byte;
                } while (other > 0);

                return *this;
            }
            /**
             * @brief Converts the varint to a uint32_t.
             * @return The varint as a uint32_t.
             * @note This function converts the varint to a uint32_t by checking the size and the
             * last byte.
             */
            operator uint32_t() const {
                if (size == 0 || size > 4)
                    return 0;

                uint32_t multiplier = 1;
                uint32_t result = 0;

                for (uint8_t i = 0; i < size; ++i)
                {
                    uint8_t byte = raw[i];
                    result += (byte & 0x7F) * multiplier;

                    if ((byte & 0x80) == 0)
                        break;

                    multiplier *= 128;
                }
                return result;
            }
            /**
             * @brief Checks if the varint is valid.
             * @return True if the varint is valid, false otherwise.
             * @note This function checks if the varint is valid by checking the size and the last
             * byte.
             */
            bool checkImpl() const override {
                return size > 0 && size < 5 && (raw[size - 1] & 0x80) == 0;
            }
            /**
             * @brief Checks if the varint is valid.
             * @return True if the varint is valid, false otherwise.
             * @note This function checks if the varint is valid by checking the size and the last
             * byte.
             */
            uint32_t getSerializedSize() const override { return size; }

            uint32_t serialize(uint8_t* buffer) override {
                std::memcpy(buffer, raw.data(), size);
                return size;  // Retourne la taille totale des données sérialisées // Return the
                              // size of the varint
            }                 //!< Serialize the varint into the buffer

            uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
                for (size = 0;;)
                {
                    if ((uint32_t)(size + 1) > bufferSize)
                        return NotEnoughData;  // Not enough data
                    raw[size] = buffer[size];
                    if (raw[size++] < 0x80)
                        break;  // Last byte has continuation bit set
                    if (size == 4)
                        return BadData;  // Invalid size
                }
                return size;  // Return the size of the varint
            }
            /** Default  */
            VBInt(uint32_t value = 0) { this->operator=(value); }
            /** This is the copy constructor  */
            VBInt(const VBInt& other) : word(other.word), size(other.size) {}
        };

        struct String
        {
            uint16_t length;  //!< The length of the string
            uint8_t* data;    //!< The data of the string

            void swapNetworkOrder() {
                length = BigEndian(length);  // Swap the length to network order
            }                                //!< Swap the string to network order
        };

        struct DynamicString : public ISerializable
        {
            uint8_t* data;  //!< The data of the string
            uint16_t size;  //!< The size of the string

            uint32_t getSerializedSize() const override {
                return 2 + (uint32_t)size;  // 2 bytes for size field + string size
            }

            uint32_t serialize(uint8_t* buffer) override {
                if (!buffer)
                    return BadData;
                uint16_t networkSize = BigEndian((uint16_t)size);
                std::memcpy(buffer, &networkSize, sizeof(networkSize));  // Copy size
                std::memcpy(buffer + sizeof(networkSize), data,
                            size);  // Copy string data
                return (uint32_t)size + 2;
            }

            uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
                if (bufferSize < sizeof(uint16_t))
                    return NotEnoughData;  // Not enough data for size
                uint16_t networkSize = 0;
                std::memcpy(&networkSize, buffer, sizeof(networkSize));
                size = BigEndian(networkSize);  // Read size from buffer
                if (bufferSize < size + sizeof(uint16_t))
                    return NotEnoughData;                            // Not enough data for content
                delete[] data;                                       // Free existing memory
                data = new uint8_t[size];                            // Allocate new memory
                std::memcpy(data, buffer + sizeof(uint16_t), size);  // Copy string data
                return (uint32_t)size + 2;
            }

            bool checkImpl() const override {
                if (size == 0)
                    return false;  // No data
                if (size > 0xFFFF)
                    return false;  // Invalid size
                return true;
            }  //!< Check the string for validity

            /**
             * * @brief Default constructor for the DynamicString class.
             */

            DynamicString(uint8_t* data = nullptr, uint32_t size = 0) : data(data), size(size) {}

            DynamicString(const char* str) {
                size = (uint8_t)strlen(str);   // Get the size of the string
                data = new uint8_t[size];      // Allocate memory for the string
                std::memcpy(data, str, size);  // Copy the string to the data
            }                                  //!< Constructor for the DynamicString class

            DynamicString(const std::string& str) {
                size = (uint8_t)str.length();          // Get the size of the string
                data = new uint8_t[size];              // Allocate memory for the string
                std::memcpy(data, str.c_str(), size);  // Copy the string to the data
            }                                          //!< Constructor for the DynamicString class

            DynamicString(const DynamicString& other) {
                size = other.size;                    // Get the size of the string
                data = new uint8_t[size];             // Allocate memory for the string
                std::memcpy(data, other.data, size);  // Copy the string to the data
            }  //!< Copy constructor for the DynamicString class

            ~DynamicString() {
                if (data != nullptr)
                    delete[] data;  // Delete the data if it is not null
            }                       //!< Destructor for the DynamicString class

            bool operator==(const DynamicString& other) const {
                if (size != other.size)
                    return false;                                 // The sizes are different
                return std::memcpy(data, other.data, size) == 0;  // Compare the data
            }  //!< Compare the strings for equality

            bool operator!=(const DynamicString& other) const {
                return !(*this == other);  // Compare the strings for inequality
            }                              //!< Compare the strings for inequality

            DynamicString& operator=(const DynamicString& other) {
                if (this != &other)
                {
                    if (data != nullptr)
                        delete[] data;                    // Delete the data if it is not null
                    size = other.size;                    // Get the size of the string
                    data = new uint8_t[size];             // Allocate memory for the string
                    std::memcpy(data, other.data, size);  // Copy the string to the data
                }
                return *this;  // Return the string
            }
            //!< Assignment operator for the DynamicString class
            void from(const char* str, size_t size = 0) {
                size = size ? size : (strlen(str) + 1);  // Get the size of the string
                data = (uint8_t*)malloc(size);           // Allocate memory for the string
                std::memcpy(data, str, size);            // Copy the string to the data
                data[size - 1] = '\0';                   // Add the null terminator to the string
            }  //!< Convert the string from a C-style string
        };

        struct DynamicStringPair : public ISerializable
        {
            DynamicString first;   //!< The first string of the pair
            DynamicString second;  //!< The second string of the pair

            uint32_t getSerializedSize() const override {
                return first.getSerializedSize() + second.getSerializedSize();
            }

            uint32_t serialize(uint8_t* buffer) override {
                uint32_t offset = 0;
                offset += first.serialize(buffer + offset);   // Serialize the first string
                offset += second.serialize(buffer + offset);  // Serialize the second string
                return offset;
            }

            uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
                if (!buffer)
                {
                    return BadData;  // Invalid buffer
                }
                uint32_t offset = 0;
                uint32_t result = first.deserialize(buffer + offset, bufferSize);
                if (result == NotEnoughData || result == BadData)
                {
                    return result;  // Propagate the error
                }
                offset += result;

                result = second.deserialize(buffer + offset, bufferSize - offset);
                if (result == NotEnoughData || result == BadData)
                {
                    return result;  // Propagate the error
                }
                offset += result;

                return offset;
            }

            bool checkImpl() const override {
                if (!first.checkImpl())
                    return false;  //!< Check the first string for validity
                if (!second.checkImpl())
                    return false;  //!< Check the second string for validity
                return true;       //!< Return true if both strings are valid
            }                      //!< Check the pair for validity
            DynamicStringPair() : first(), second() {}

            DynamicStringPair(const DynamicString& first, const DynamicString& second) :
                first(first), second(second) {}  //!< Constructor for the DynamicStringPair class
            DynamicStringPair(const char* first, const char* second) :
                first(first), second(second) {}  //!< Constructor for the DynamicStringPair class
            DynamicStringPair(const std::string& first, const std::string& second) :
                first(first), second(second) {}  //!< Constructor for the DynamicStringPair class
            DynamicStringPair(const std::string& first, const char* second) :
                first(first), second(second) {}  //!< Constructor for the DynamicStringPair class
            DynamicStringPair(const char* first, const std::string& second) :
                first(first), second(second) {}  //!< Constructor for the DynamicStringPair class
            DynamicStringPair(const DynamicStringPair& other) :
                first(other.first),
                second(other.second) {}  //!< Copy constructor for the DynamicStringPair class
            DynamicStringPair(DynamicStringPair&& other) :
                first(std::move(other.first)),
                second(std::move(other.second)) {
            }  //!< Move constructor for the DynamicStringPair class
            DynamicStringPair& operator=(const DynamicStringPair& other) {
                if (this != &other)
                {
                    first = other.first;    //!< Assign the first string of the pair
                    second = other.second;  //!< Assign the second string of the pair
                }
                return *this;  //!< Return the pair
            }                  //!< Assignment operator for the DynamicStringPair class
            DynamicStringPair& operator=(DynamicStringPair&& other) {
                if (this != &other)
                {
                    first = std::move(other.first);    //!< Move the first string of the pair
                    second = std::move(other.second);  //!< Move the second string of the pair
                }
                return *this;  //!< Return the pair
            }                  //!< Move assignment operator for the DynamicStringPair class
            ~DynamicStringPair() = default;  //!< Destructor for the DynamicStringPair class
        };

        struct DynamicStringView final : public ISerializable
        {
            const char* data;  //!< The data of the dynamic string view
            uint16_t size;     //!< The size of the dynamic string view

            uint32_t getSerializedSize() const override {
                return size + sizeof(uint16_t);
            }  //!< Get the size of the dynamic string view

            uint32_t serialize(uint8_t* buffer) override {
                if (!buffer)
                    return BadData;  // Invalid buffer
                uint16_t networkSize = BigEndian(size);
                std::memcpy(buffer, &networkSize, sizeof(networkSize));  // Copy size
                std::memcpy(buffer + sizeof(networkSize), data, size);   // Copy data
                return sizeof(networkSize) + size;
            }  //!< Serialize the dynamic string view into the buffer

            uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
                if (bufferSize < sizeof(uint16_t))
                    return NotEnoughData;  // Not enough data
                uint16_t networkSize = 0;
                std::memcpy(&networkSize, buffer, sizeof(networkSize));
                size = BigEndian(networkSize);  // Swap the length to network order
                if (bufferSize < size + sizeof(uint16_t))
                    return NotEnoughData;  // Not enough data

                data = reinterpret_cast<const char*>(buffer +
                                                     sizeof(uint16_t));  // Set the data pointer
                return sizeof(uint16_t) + size;  // Return the size of the dynamic string view
            }  //!< Deserialize the dynamic string view from the buffer

            bool checkImpl() const override { return (data != nullptr || size == 0); }

            DynamicStringView(const char* data = nullptr, uint16_t size = 0) :
                data(data), size(size) {}

            DynamicStringView(const char* str) :
                data(str),
                size((uint32_t)strlen(str)) {}  //!< Constructor for the DynamicStringView class
            explicit DynamicStringView(std::string_view str) :
                data(str.data()), size(static_cast<uint16_t>(str.size())) {}

            ~DynamicStringView() = default;
        };

        /**
         * @brief A structure for dynamic string pair view (viewing existing data).
         */
        struct DynamicStringPairView : public ISerializable
        {
            DynamicStringView first;   //!< The first string in the pair (view)
            DynamicStringView second;  //!< The second string in the pair (view)

            uint32_t getSerializedSize() const override {
                return first.getSerializedSize() + second.getSerializedSize();
            }

            uint32_t serialize(uint8_t* buffer) override {
                uint32_t offset = first.serialize(buffer);
                offset += second.serialize(buffer + offset);
                return offset;
            }

            uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
                uint32_t offset = first.deserialize(buffer, bufferSize);
                if (offset >= Shortcut)
                { return offset; }
                uint32_t offset2 = second.deserialize(buffer + offset, bufferSize - offset);
                if (offset2 >= Shortcut)
                { return offset2; }
                return offset + offset2;
            }

            bool checkImpl() const override { return first.checkImpl() && second.checkImpl(); }
            DynamicStringPairView() {}
            DynamicStringPairView(const DynamicStringView first, const DynamicStringView second) :
                first(first), second(second) {}
        };

        /**
         * @brief A structure that represents a binary data.
         */
        struct BinaryData
        {
            uint8_t* data;  //!< The data of the binary data
            uint16_t size;  //!< The size of the binary data

            void swapNetworkOrder() {
                for (uint32_t i = 0; i < size; i++)
                    data[i] = BigEndian(data[i]);  // Swap the data to network order
            }                                      //!< Swap the binary data to network order

            BinaryData(uint8_t* data = nullptr, uint16_t size = 0) :
                data(data), size(size) {}  //!< Constructor for the BinaryData class
            ~BinaryData() {
                if (data != nullptr)
                    delete[] data;  // Delete the data if it is not null
            }                       //!< Destructor for the BinaryData class
        };

        struct DynamicBinaryData : public ISerializable
        {
            uint8_t* data;  //!< The data of the dynamic binary data
            uint16_t size;  //!< The size of the dynamic binary data

            uint32_t getSerializedSize() const override {
                return sizeof(uint16_t) + size;  //!< Get the size of the dynamic binary data
            }                                    //!< Get the size of the dynamic binary data

            bool checkImpl() const override {
                return data != nullptr && size > 0 && size <= 0xFFFF;
            }
            //!< Check the dynamic binary data for validity

            uint32_t serialize(uint8_t* buffer) override {
                if (!buffer)
                    return BadData;  // Invalid buffer
                uint16_t networkSize = BigEndian((uint16_t)size);
                std::memcpy(buffer, &networkSize, sizeof(networkSize));  // Copy size
                std::memcpy(buffer + sizeof(networkSize), data, size);   // Copy data
                return sizeof(networkSize) + size;
            }  //!< Serialize the dynamic binary data into the buffer

            uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
                if (!buffer || bufferSize < sizeof(uint16_t))
                    return NotEnoughData;

                uint16_t netSize;
                std::memcpy(&netSize, buffer, sizeof(uint16_t));
                size = BigEndian(netSize);

                if (bufferSize < size + sizeof(uint16_t))
                    return NotEnoughData;

                delete[] data;
                data = new uint8_t[size];
                std::memcpy(data, buffer + sizeof(uint16_t), size);

                return sizeof(uint16_t) + size;
            }

            DynamicBinaryData& operator=(const DynamicBinaryData& other) {
                if (this != &other)
                {
                    if (data != nullptr)
                        delete[] data;         // Delete the data if it is not null
                    size = other.size;         // Get the size of the dynamic binary data
                    data = new uint8_t[size];  // Allocate memory for the dynamic binary data
                    std::memcpy(data, other.data,
                                size);  // Copy the dynamic binary data to the data
                }
                return *this;  // Return the dynamic binary data
            }                  //!< Assignment operator for the DynamicBinaryData class

            DynamicBinaryData& operator=(DynamicBinaryData&& other) noexcept {
                if (this != &other)
                {
                    delete[] data;  // libérer les anciens
                    data = other.data;
                    size = other.size;
                    other.data = nullptr;
                    other.size = 0;
                }
                return *this;
            }

            DynamicBinaryData(const DynamicBinaryData& other) {
                if (size > 0 && other.data)
                {
                    data = new uint8_t[size];
                    std::memcpy(data, other.data, size);
                }
            }

            DynamicBinaryData(DynamicBinaryData&& other) noexcept :
                data(other.data), size(other.size) {
                other.data = nullptr;
                other.size = 0;
            }

            /**
             * @brief Default constructor for the DynamicBinaryData class.
             * @param data The data of the dynamic binary data.
             * @param size The size of the dynamic binary data.
             */
            DynamicBinaryData() : data(nullptr), size(0) {}
            DynamicBinaryData(const uint8_t* src, uint16_t sz) : data(nullptr), size(sz) {
                if (size > 0 && src)
                {
                    data = new uint8_t[size];
                    std::memcpy(data, src, size);
                }
            }

            ~DynamicBinaryData() {
                delete[] data;
                size = 0;
            }  //!< Destructor for the DynamicBinaryData class
        };

        /**
         * @brief A structure for dynamic binary data view (viewing existing data).
         */
        struct DynamicBinaryDataView : public ISerializable
        {
            const uint8_t* data;  //!< The binary data (view)
            uint32_t size;        //!< The size of the binary data

            uint32_t getSerializedSize() const override {
                return sizeof(uint16_t) + size;  // 2 bytes for size field + binary data size
            }

            uint32_t serialize(uint8_t* buffer) override {
                if (!buffer)
                    return BadData;
                uint16_t networkSize = BigEndian((uint16_t)size);
                std::memcpy(buffer, &networkSize, sizeof(networkSize));  // Copy size
                std::memcpy(buffer + sizeof(networkSize), data, size);   // Copy binary data
                return getSerializedSize();
            }

            uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
                if (bufferSize < sizeof(uint16_t))
                    return 0;
                size = BigEndian(*(uint16_t*)buffer);  // Read size
                if (bufferSize < size + sizeof(uint16_t))
                    return 0;
                data = buffer + sizeof(uint16_t);  // Point to binary data
                return getSerializedSize();
            }

            bool checkImpl() const override { return true; }

            DynamicBinaryDataView(const uint8_t* data = nullptr, uint32_t size = 0) :
                data(data), size(size) {}
        };

        struct MappedVBInt : public ISerializable
        {
            const uint8_t* data;  //!< The data of the mapped varint
            uint16_t size;        //!< The size of the mapped varint

            uint32_t getSerializedSize() const override {
                return size;
            }  //!< Get the size of the mapped varint

            uint32_t serialize(uint8_t* buffer) override {
                if (!buffer || !data)
                    return BadData;
                std::memcpy(buffer, data, size);
                return size;  // Return the size of the mapped varint
            }                 //!< Serialize the mapped varint into the buffer

            uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) override {
                if (!buffer)
                    return BadData;

                size = 0;
                while (size < bufferSize && size < 4)
                {
                    ++size;
                    if ((buffer[size - 1] & 0x80) == 0)
                        break;  // dernier octet
                }

                if (size > bufferSize)
                    return NotEnoughData;

                data = buffer;
                return size;  // Return the size of the mapped varint
            }                 //!< Deserialize the mapped varint from the buffer

            bool checkImpl() const override {
                return size > 0 && size <= 4 && data != nullptr && (data[size - 1] & 0x80) == 0;
            }

            MappedVBInt(const uint8_t* data = nullptr, uint16_t size = 0) :
                data(data), size(size) {}
        };
    }  // namespace Common

    namespace Mqtt_V5
    {
        using namespace Common;  //!< Use the Common namespace

        struct FixedHeader
        {
            union {
                uint8_t raw;
                BitField<uint8_t, 4, 4> type;        //!< The type of the fixed header
                BitField<uint8_t, 3, 1> duplicated;  //!< The reserved bits of the fixed header
                BitField<uint8_t, 1, 2> qos;         //!< The QoS level of the fixed header
                BitField<uint8_t, 0, 1> retain;      //!< The retain flag of the fixed header
            };
        };

        struct FixedHeaderBase
        {
            uint8_t typeandFlags;  //!< The type and flags of the fixed header

            FixedHeaderBase(const ControlPacketType type, const uint8_t flags) :
                typeandFlags((uint8_t)((uint8_t)type << 4) | (flags & 0xF)) {
            }  //!< Constructor for the FixedHeaderBase class
            FixedHeaderBase() {}
            ~FixedHeaderBase() {}  //!< Destructor for the FixedHeaderBase class

            virtual ControlPacketType getType() const {
                return (ControlPacketType)(typeandFlags >> 4);
            }  //!< Get the type of the fixed header
            virtual uint8_t getFlags() const {
                return typeandFlags & 0xF;
            }                                              //!< Get the flags of the fixed header
            virtual bool isValid() const { return true; }  //!< Check if the fixed header is valid
        };

        template <ControlPacketType type, uint8_t flags>
        struct FixedHeaderType final : public FixedHeaderBase
        {
            uint32_t getSerializedSize() const { return sizeof(typeandFlags); }

            uint32_t serialize(uint8_t* buffer) const {
                if (!buffer)
                { return BadData; }
                buffer[0] = typeandFlags;
                return sizeof(typeandFlags);
            }
            uint32_t deserialize(const uint8_t* buffer, uint32_t bufferSize) {
                if (!buffer || bufferSize < sizeof(typeandFlags))
                { return NotEnoughData; }
                typeandFlags = buffer[0];
                return sizeof(typeandFlags);
            }

            bool isValid() const override {
                return getFlags() == flags;
            }  //!< Check if the fixed header is valid

            static bool isValidFalag(const uint8_t flag) {
                return flag == flags;
            }  //!< Check the flag

            FixedHeaderType() :
                FixedHeaderBase(type, flags) {}  //!< Constructor for the FixedHeaderType class
        };

        template <>
        struct FixedHeaderType<PUBLISH, 0> final : public FixedHeaderBase
        {
            FixedHeaderType(const uint8_t Flags = 0) :
                FixedHeaderBase(PUBLISH, Flags) {}  //!< Constructor for the FixedHeaderType class
            FixedHeaderType(const bool duplicated, const bool retained, const uint8_t qos) :
                FixedHeaderBase(PUBLISH, (duplicated ? 0x08 : 0x00) | (retained ? 0x01 : 0x00) |
                                             ((qos & 0x03) << 1)) {
            }  //!< Constructor for the FixedHeaderType class

            bool isDuplicated() const {
                return typeandFlags & 0x8;
            }  //!< Check if the fixed header is duplicated
            bool isRetained() const {
                return typeandFlags & 0x1;
            }  //!< Check if the fixed header is retained
            uint8_t getQoS() const {
                return (typeandFlags & 0x6) >> 1;
            }  //!< Get the QoS level of the fixed header

            void setDuplicated(const bool x) {
                typeandFlags = (typeandFlags & ~0x08) | (x ? 0x08 : 0x00);
            }  //!< Set the duplicated flag of the fixed header
            void setRetained(const bool x) {
                typeandFlags = (typeandFlags & ~0x01) | (x ? 0x01 : 0x00);
            }
            void setQos(const uint8_t x) {
                typeandFlags = typeandFlags & ~0x06 | (x < 0x03 ? (x << 0x01) : 0x00);
            }

            static bool isValidFalag(const uint8_t flag) { return true; }  //!< Check the flag
        };

        typedef FixedHeaderType<CONNECT, 0>
            ConnectFixedHeader;  //!< Type for the connect fixed header
        typedef FixedHeaderType<CONNACK, 0>
            ConnAckFixedHeader;  //!< Type for the connack fixed header
        typedef FixedHeaderType<PUBLISH, 0>
            PublishFixedHeader;  //!< Type for the publish fixed header
        typedef FixedHeaderType<PUBACK, 0> PubAckFixedHeader;  //!< Type for the puback fixed header
        typedef FixedHeaderType<PUBREC, 0> PubRecFixedHeader;  //!< Type for the pubrec fixed header
        typedef FixedHeaderType<PUBREL, 2> PubRelFixedHeader;  //!< Type for the pubrel fixed header
        typedef FixedHeaderType<PUBCOMP, 0>
            PubCompFixedHeader;  //!< Type for the pubcomp fixed header
        typedef FixedHeaderType<SUBSCRIBE, 2>
            SubscribeFixedHeader;  //!< Type for the subscribe fixed header
        typedef FixedHeaderType<SUBACK, 0> SubAckFixedHeader;  //!< Type for the suback fixed header
        typedef FixedHeaderType<UNSUBSCRIBE, 2>
            UnsubscribeFixedHeader;  //!< Type for the unsubscribe fixed header
        typedef FixedHeaderType<UNSUBACK, 0>
            UnsubAckFixedHeader;  //!< Type for the unsuback fixed header
        typedef FixedHeaderType<PINGREQ, 0>
            PingReqFixedHeader;  //!< Type for the pingreq fixed header
        typedef FixedHeaderType<PINGRESP, 0>
            PingRespFixedHeader;  //!< Type for the pingresp fixed header
        typedef FixedHeaderType<DISCONNECT, 0>
            DisconnectFixedHeader;                         //!< Type for the disconnect fixed header
        typedef FixedHeaderType<AUTH, 0> AuthFixedHeader;  //!< Type for the auth fixed header

        static inline uint32_t checkHeader(const uint8_t* buffer, const uint32_t bufferSize,
                                           ControlPacketType* type = nullptr) {
            if (bufferSize < 2)
                return NotEnoughData;  // Not enough data
            std::vector<uint8_t> expectedFlugs = {
                0xF, 0, 0xF, 0, 0, 2, 0, 2,
                0,   2, 0,   0, 0, 0, 0, 0};  // Expected flags for each type
            if (((*buffer & 0xF0) ^ (expectedFlugs[(*buffer >> 4)])) != 0x10 &&
                ((*buffer >> 4) != ControlPacketType::PUBLISH))
                return BadData;  // Invalid data
            if (type)
                *type = (ControlPacketType)(*buffer & 0xF0);  // Get the type of the fixed header
            VBInt remainingLength;
            uint32_t size = remainingLength.deserialize(buffer + 1, bufferSize - 1);
            if (size == NotEnoughData)
                return NotEnoughData;  // Not enough data
            if (size == BadData)
                return BadData;  // Invalid data
            return size;         // Return the size of the fixed header
        };                       // Expected flags for each type
    }                            // namespace Mqtt_V5
}  // namespace MqttV5
#endif /* MQTT_MQTTUTILS_HPP */
