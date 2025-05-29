/**
 * @file MqttClient.cpp
 * @brief This file contains the implementation of the MqttClient class.
 * @note This class is used to connect to an MQTT broker and send/receive messages.
 * @note This class is used to store packets in a circular buffer.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */

#include "MqttV5/MqttV5Core/MqttClient.hpp"

namespace
{
    constexpr double DEFAULT_REQUEST_TIMEOUT_SECONDS = 30.0;  // Default request timeout in seconds
    constexpr double DEFAULT_INACTIVITY_INTERVAL_SECONDS =
        60.0;  // Default inactivity interval in seconds
    struct MqttOptions
    {
        /**
         * Set to true to use authentication with the broker.
         * Default: false
         */
        bool useAuthentication = false;
        /**
         * Set to true to reject unauthorized connections.
         * This is used to reject connections to brokers that do not have a valid certificate.
         * Default: false
         */
        bool rejectUnauthorized = false;
        /** Remove all validation from MQTT types.
         * This removes validation check for all MQTT types in order to save binary size.
         * This is only recommanded if you are sure about your broker implementation (don't set this
         * to 1 if you intend to connect to unknown broker) Default: 0
         * */
        bool avoidValidation = false;
        /**
         * Set to:
         * - 1 for enable complete QoS management. This imply allocating and using a buffer that's
         * 3x the maximum packet size you've set (for saving QoS packets against disconnection) plus
         * a small overhead in binary size.
         * - 0 to implement a non compliant, but QoS capable client. No packet are saved for QoS,
         * but the client will claim and follow QoS protocols for publish cycle. The binary overhead
         * is also reduced.
         * - -1 to disable QoS management. The client will never claim it is QoS capable. Saves the
         * maximum binary size. Default: 1
         */
        int qosSupportLevel = 1;
        /**
         * Set to true to use SSL for the connection.
         * Default: false
         */
        bool useSSL = false;
        /**
         * Set to true to use TLS for the connection.
         * Default: false
         */
        bool useTLS = false;
        /**
         * The period in milliseconds to wait before attempting to reconnect to the broker.
         * Default: 1000 milliseconds
         */
        uint32_t reconnectPeriod = 1000;  // milliseconds
        /**
         * The timeout in milliseconds to wait for a connection to the broker.
         * Default: 30000 milliseconds
         */
        uint32_t connectTimeout = 30000;  // milliseconds
        /**
         * Set to true to resubscribe to topics after reconnecting to the broker.
         * Default: true
         */
        bool resubscribe = true;  // Resubscribe to topics after reconnect
    };

    struct ClientConnectionState
    {
        /**
         * This is the current state of the connection.
         * It is used to track the connection state of the client.
         */
        std::shared_ptr<MqttV5::Connection> connection;

        /**
         * This is the current transaction that is being processed.
         * It is used to track the current transaction state of the client.
         */
        std::weak_ptr<struct Transaction> currentTransaction;

        /**
         * This indicates if the connection is broken.
         * If true, the client will attempt to reconnect to the broker.
         */
        bool broken = false;  // This is used to track if the connection is broken
    };
}  // namespace

namespace MqttV5
{
    struct PacketBookmark
    {
        uint16_t id;
        uint32_t size;
        uint32_t pos;

        inline void Set(uint16_t id, uint32_t size, uint32_t pos) {
            this->id = id;
            this->size = size;
            this->pos = pos;
        }
        PacketBookmark(uint16_t id = 0, uint32_t size = 0, uint32_t pos = 0) :
            id(id), size(size), pos(pos) {}
    };

    struct Storage::CircularBufferStore::Impl
    {
        uint32_t read, write;

        const uint32_t bufferSize;

        uint8_t* buffer;

        PacketBookmark* bookmarks;

        uint32_t maxPacketsCount;  // Maximum number of packets that can be stored

        uint32_t FindId(uint32_t id) const {
            for (uint32_t i = 0; i < maxPacketsCount; ++i)
            {
                if (bookmarks[i].id == id)
                { return i; }
            }
            return maxPacketsCount;
        }

        /**
         * get the consumed size of the buffer
         */
        inline uint32_t GetConsumedSize() const {
            return (read <= write) ? write - read : bufferSize - (read - write - 1);
        }

        /**
         * get the free size of the buffer
         */
        inline uint32_t GetFreeSize() const { return bufferSize - GetConsumedSize(); }

        bool SavePacket(uint32_t packetID, uint32_t packetSize, const uint8_t* packet) {
            if (packetSize > bufferSize || GetFreeSize() < packetSize)
            {
                return false;  // Not enough space to store the packet
            }

            uint32_t index = FindId(0);

            if (index == maxPacketsCount)
            { return false; }

            const uint32_t p1 = std::min(packetSize, bufferSize - write + 1);
            const uint32_t p2 = packetSize - p1;

            memcpy(buffer + write, packet, p1);
            memcpy(buffer, packet + p1, p2);

            bookmarks[index].Set(packetID, packetSize, write);
            write = (write + packetSize) & bufferSize;
            return true;
        }

        // bool LoadPacket(uint32_t packetID, uint32_t& packetSize, uint8_t*& packet) {
        //     // Look for the packet in bookmarks
        //     auto index = FindId(packetID);
        //     if (index == maxPacketsCount)
        //         return false;

        //     auto position = bookmarks[index].pos;
        //     auto sizeHead =
        //         std::min(bookmarks[index].size, (bufferSize - bookmarks[index].pos + 1));
        //     auto sizeTail = bookmarks[index].size - sizeHead;

        //     packetSize = sizeHead + sizeTail;
        //     packet = new uint8_t[packetSize];
        //     memcpy(packet, buffer + position, sizeHead);
        //     if (sizeTail > 0)
        //     { memcpy(packet + sizeHead, buffer, sizeTail); }
        //     return true;
        // }

        bool LoadPacket(uint32_t packetID, uint32_t& headerSize, uint8_t*& headerPacket,
                        uint32_t& tailSize, uint8_t*& tailPacket) {
            auto index = FindId(packetID);
            if (index == maxPacketsCount)
                return false;

            auto position = bookmarks[index].pos;
            headerPacket = buffer + position;
            headerSize = std::min(bookmarks[index].size, (bufferSize - position + 1));
            tailSize = bookmarks[index].size - headerSize;
            tailPacket = buffer;
            return true;
        }

        bool RemovePacket(const uint32_t packetID) {
            uint8_t index = FindId(packetID);
            if (index == maxPacketsCount)
            {
                return false;  // Packet not found
            }

            PacketBookmark& packet = bookmarks[index];

            // Adjust read pointer if necessary
            uint32_t position = packet.pos;
            uint32_t size = packet.size;
            uint32_t end = (packet.pos + packet.size) & bufferSize;
            // Clear the bookmark
            packet.Set(0, 0, 0);
            if (position == read)
            {
                // Advence read pointer if packet was at head.
                read = (read + size) & bufferSize;
                return true;
            }

            if (end == write)
            {
                // Rewind write pointer if packet was last written.
                write = position;
                return true;
            }

            // Move memory to fill the gap.
            uint32_t s = bufferSize + 1;
            uint32_t W = (write < position ? write + s : write);
            uint32_t E = (end < position ? end + s : end);

            for (uint32_t j = 0; j < W - E; j++)
            { buffer[(j + position) & bufferSize] = buffer[(j + position + size) & bufferSize]; }

            write = (W - size) & bufferSize;

            // Update metadata for affected packets
            bool searching = true;
            while (searching)
            {
                searching = false;
                for (uint8_t j = 0; j < maxPacketsCount; j++)
                {
                    PacketBookmark& iter = bookmarks[j];
                    if (iter.pos == end)
                    {
                        end = (iter.pos + iter.size) & bufferSize;
                        iter.pos = position;
                        position = (iter.pos + iter.size) & bufferSize;
                        searching = true;
                        break;
                    }
                }
            }

            return true;
        }

        Impl(uint8_t* buffer, uint32_t bufferSize, uint32_t maxPacketsCount,
             PacketBookmark* bookmarks) :
            buffer(buffer),
            read(0),
            write(0),
            bufferSize(bufferSize - 1),
            maxPacketsCount(maxPacketsCount),
            bookmarks(bookmarks) {}
        ~Impl() {
            delete[] buffer;
            delete[] bookmarks;
        }
    };

    bool Storage::CircularBufferStore::StorePacket(uint32_t packetID, uint32_t packetSize,
                                                   const uint8_t* packet) {
        if (!impl_)
        { return false; }  // Not initialized

        return impl_->SavePacket(packetID, packetSize, packet);
    }

    // bool Storage::CircularBufferStore::RetrievePacket(uint32_t packetID, uint32_t& packetSize,
    //                                                   uint8_t*& packet) {
    //     if (!impl_)
    //     { return false; }  // Not initialized

    //     return impl_->LoadPacket(packetID, packetSize, packet);
    // }

    bool Storage::CircularBufferStore::RetrievePacket(uint32_t packetID, uint32_t& headSize,
                                                      uint8_t*& headPacket, uint32_t tailSize,
                                                      uint8_t*& tailPacket) {
        if (!impl_)
        { return false; }  // Not initialized

        return impl_->LoadPacket(packetID, headSize, headPacket, tailSize, tailPacket);
    }

    bool Storage::CircularBufferStore::RemovePacket(uint32_t packetID) {
        if (!impl_)
        { return false; }  // Not initialized

        return impl_->RemovePacket(packetID);
    }
    Storage::CircularBufferStore::CircularBufferStore(const uint32_t size,
                                                      const uint32_t maxPacketsCount) :
        impl_(std::make_unique<Impl>(new uint8_t[size], size, maxPacketsCount,
                                     new PacketBookmark[maxPacketsCount])) {}
    Storage::CircularBufferStore::~CircularBufferStore() {  // Ensure the implementation is cleaned
                                                            // up
    }

    struct MqttClient::Impl
    {
        /**
         * This is the delegate to generate diagnostic messages
         * for the client.
         */
        SystemUtils::DiagnosticsSender diagnosticSender;
        /**
         * This is the options structure for the client.
         * It contains all the options that can be set for the client.
         */
        MqttOptions options;

        bool mobilized = false;  // This is used to track if the client is mobilized

        // This is the transport layer implementation to use.
        std::shared_ptr<ClientTransportLayer> transport;

        // This is the object used to track time in the client.
        std::shared_ptr<TimeKeeper> timeKeeper;

        // This is the amount of time after a request is made
        // of a server, before the transaction is considered timed out
        // if no part of a response has been received.
        const double requestTimeoutSeconds = DEFAULT_REQUEST_TIMEOUT_SECONDS;

        // This is the amount of time, after a transaction is completed,
        // that a persistent connection is closed if another transaction
        // does not reuse the connection.
        const double inactivityInterval = DEFAULT_INACTIVITY_INTERVAL_SECONDS;

        // Lifecycle management

        ~Impl() noexcept = default;
        Impl(const Impl&) = delete;
        Impl(Impl&&) noexcept = delete;
        Impl& operator=(const Impl&) = delete;
        Impl& operator=(Impl&&) noexcept = delete;

        // Methods

        /**
         * This is the constructor for the structure.
         */
        Impl() : diagnosticSender("MqttV5::MqttClient") {}
    };
}  // namespace MqttV5
