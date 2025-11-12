/**
 * @file MqttCircularBufferTest.cpp
 * @brief This file contains the implementation of the MqttCircularBufferTest class.
 * * @note This class is used to test the MqttCircularBuffer class.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */

#include "MqttV5/MqttClient.hpp"
#include <gtest/gtest.h>

namespace
{
    bool isPowerOfTwo(uint32_t n) { return (n != 0) && ((n & (n - 1)) == 0); }
    uint32_t random(uint32_t size) { return std::rand() % size; }
}  // namespace

struct MqttCircularBufferTest : public ::testing::Test
{
    virtual void SetUp() override {
        // This method is called before each test
        bufferSize = 2048 * 4;
        maxPacketsCount = 4;

        if (isPowerOfTwo(bufferSize))
        {
            circularBuffer =
                std::make_unique<MqttV5::Storage::CircularBufferStore>(bufferSize, maxPacketsCount);
        } else
        { return; }
    }

    virtual void TearDown() override {
        // This method is called after each test
        circularBuffer.reset();
    }
    // Member variables
    uint32_t bufferSize;
    uint32_t maxPacketsCount;
    std::unique_ptr<MqttV5::Storage::CircularBufferStore> circularBuffer;
};

TEST_F(MqttCircularBufferTest, MqttCircularBufferTest_StoreAndRetrievePacket_Test) {
    uint32_t packetID = 1;
    const uint32_t packetSize = 128;
    uint8_t packetData[packetSize] = {0};

    // Store the packet
    EXPECT_TRUE(circularBuffer->StorePacket(packetID, packetSize, packetData));

    // Retrieve the packet
    // uint32_t retrievedSize;
    uint8_t *headerPacket = nullptr;
    uint8_t *tailPacket = nullptr;
    uint32_t headerSize = 0;
    uint32_t tailSize = 0;
    EXPECT_TRUE(
        circularBuffer->RetrievePacket(packetID, headerSize, headerPacket, tailSize, tailPacket));
    EXPECT_EQ(headerSize + tailSize, packetSize);
    EXPECT_NE(headerPacket, nullptr);
}

TEST_F(MqttCircularBufferTest, MqttCircularBufferTest_RemovePacket_Test) {
    uint32_t packetID = 2;
    const uint32_t packetSize = 64;
    uint8_t packetData[packetSize] = {0};

    // Store the packet
    EXPECT_TRUE(circularBuffer->StorePacket(packetID, packetSize, packetData));

    // Remove the packet
    EXPECT_TRUE(circularBuffer->RemovePacket(packetID));
    uint8_t *headerPacket = nullptr;
    uint8_t *tailPacket = nullptr;
    uint32_t headerSize = 0;
    uint32_t tailSize = 0;
    EXPECT_FALSE(
        circularBuffer->RetrievePacket(packetID, headerSize, headerPacket, tailSize, tailPacket));
    EXPECT_EQ(headerSize + tailSize, 0);
    EXPECT_EQ(nullptr, headerPacket);
    EXPECT_EQ(nullptr, tailPacket);
}

TEST_F(MqttCircularBufferTest, MqttCircularBufferTest_InsufficientSpace_Test) {
    uint32_t packetID = 3;
    const uint32_t packetSize = (2048 * 4) + 16;  // Exceeding max packet size
    uint8_t packetData[packetSize] = {0};

    // Attempt to store the packet
    EXPECT_FALSE(circularBuffer->StorePacket(packetID, packetSize, packetData));
}

TEST_F(MqttCircularBufferTest, MqttCircularBufferTest_StoreAndRetriveMultiPackets_Test) {
    uint32_t packetCount = 0;
    uint8_t buffer[2048] = {0xDE, 0xAD, 0xFA, 0xCE};
    uint8_t cleaningOrder = static_cast<uint8_t>(random(maxPacketsCount));
    uint32_t packetId = 1;
    for (int i = 0; i < 20; i++)
    {
        uint32_t size = (random(bufferSize) * 2048) / bufferSize;
        if (size < 8)
            continue;
        buffer[size - 4] = 0xB1;
        buffer[size - 3] = 0x6B;
        buffer[size - 2] = 0x00;
        buffer[size - 1] = 0x0B;
        memset(buffer + 4, packetId, size - 8);
        ASSERT_TRUE(circularBuffer->StorePacket(packetId++, size, buffer));
        packetCount++;

        if (packetCount == maxPacketsCount)
        {
            uint8_t *head = 0, *tail = 0;
            uint32_t headSize = 0, tailSize = 0;
            const auto id = packetId - packetCount + cleaningOrder;
            ASSERT_TRUE(circularBuffer->RetrievePacket(id, headSize, head, tailSize, tail));
            memcpy(buffer, head, headSize);
            memcpy(buffer + headSize, tail, tailSize);

            ASSERT_EQ(buffer[0], 0xDE);
            ASSERT_EQ(buffer[1], 0xAD);
            ASSERT_EQ(buffer[2], 0xFA);
            ASSERT_EQ(buffer[3], 0xCE);
            uint32_t s = headSize + tailSize;
            ASSERT_EQ(buffer[s - 4], 0xB1);
            ASSERT_EQ(buffer[s - 3], 0x6B);
            ASSERT_EQ(buffer[s - 2], 0x00);
            ASSERT_EQ(buffer[s - 1], 0x0B);

            ASSERT_TRUE(circularBuffer->RemovePacket(id));
            packetCount--;
        }
    }
}
