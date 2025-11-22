/**
 * @file MqttV5PacketsTests.cpp
 * @brief This module contains tests for ControlPackets building
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */
#include <memory>
#include <gtest/gtest.h>
#include <Utf8/Utf8.hpp>
#include <MqttV5/MqttV5Properties.hpp>
#include <MqttV5/MqttV5Constants.hpp>
#include <MqttV5/MqttV5Types.hpp>
#include <MqttV5/MqttV5ISerializable.hpp>
#include <MqttV5/MqttV5Packets.hpp>

using namespace MqttV5;
using namespace MqttV5::Common;
using namespace MqttV5::Mqtt_V5;

struct MqttV5PacketsTests : public ::testing::Test
{
    static Properties props;
    Utf8::Utf8 utf;

    virtual void SetUp() override {
        props.initialize();
        props.addProperty(PayloadFormatIndicator_prop::create(0x01));
        props.addProperty(MessageExpiryInterval_prop::create(120000));
        props.addProperty(ContentType_prop::create(DynamicString("application/json")));
        props.addProperty(ResponseTopic_prop::create(DynamicString("response/topic")));
        props.addProperty(CorrelationData_prop::create(
            DynamicBinaryData(utf.Encode(Utf8::AsciiToUnicode("ABCD")).data(), 4)));
        props.addProperty(UserProperty_prop::create(DynamicStringPair("key", "value")));
        props.addProperty(SubscriptionIdentifier_prop::create(123456));
        props.addProperty(SessionExpiryInterval_prop::create(7200));
        props.addProperty(ServerKeepAlive_prop::create(60));
        props.addProperty(ResponseInformation_prop::create(DynamicString("info")));
        props.addProperty(ServerReference_prop::create(DynamicString("server")));
        props.addProperty(AuthenticationMethod_prop::create(DynamicString("auth")));
        props.addProperty(AuthenticationData_prop::create(
            DynamicBinaryData(utf.Encode(Utf8::AsciiToUnicode("authData")).data(), 8)));
        props.addProperty(ReasonString_prop::create(DynamicString("reason")));
        props.addProperty(TopicAlias_prop::create(100));
        props.addProperty(TopicAliasMaximum_prop::create(200));
        props.addProperty(MaximumPacketSize_prop::create(1024 * 1024));
        props.addProperty(MaximumQoS_prop::create(1));
        props.addProperty(RetainAvailable_prop::create(1));
        props.addProperty(WildcardSubscriptionAvailable_prop::create(1));
        props.addProperty(SubscriptionIdentifierAvailable_prop::create(1));
        props.addProperty(SharedSubscriptionAvailable_prop::create(1));
        props.addProperty(ReceiveMaximum_prop::create(50));
        props.addProperty(WillDelayInterval_prop::create(10));
        props.addProperty(RequestResponseInformation_prop::create(1));
        props.addProperty(RequestProblemInformation_prop::create(1));
        props.addProperty(AssignedClientIdentifier_prop::create(DynamicString("client")));
    }

    virtual void TearDown() override {
        // Clean up any resources allocated during the tests
        props.clear();
    }
};

Properties MqttV5PacketsTests::props;

TEST_F(MqttV5PacketsTests, buildandSerializeConnectPacket) {
    const char* clientId = "client";
    const char* userName = "user";
    std::string string_pass = "pass";
    std::string willPayload = "will";
    auto encodedPass = utf.Encode(Utf8::AsciiToUnicode(string_pass));
    DynamicBinaryData password(encodedPass.data(), static_cast<uint32_t>(encodedPass.size()));

    auto encodedWill = utf.Encode(Utf8::AsciiToUnicode(willPayload));
    DynamicBinaryData will(encodedWill.data(), static_cast<uint32_t>(encodedWill.size()));

    WillMessage willMsg;
    willMsg.topicName = "will/topic";
    willMsg.payload = will;

    auto packet = PacketsBuilder::buildConnectPacket(
        clientId, userName, &password, true, 60, &willMsg, QoSDelivery::AtLeastOne, true, &props);
    // packet->computePacketSize();
    ASSERT_TRUE(packet->checkImpl());

    uint8_t buffer[1024] = {};
    uint32_t serializedSize =
        static_cast<ControlPacketSerializableImpl*>(packet)->serialize(buffer);

    ASSERT_GT(serializedSize, 0);
    ASSERT_LT(serializedSize, sizeof(buffer));

    ConnectPacket receivedPacket;
    uint32_t deserializableSize = receivedPacket.deserialize(buffer, serializedSize);

    EXPECT_EQ(deserializableSize, serializedSize);
    //    ASSERT_TRUE(receivedPacket.checkImpl());
    EXPECT_EQ(receivedPacket.payload.clientID, DynamicString("client"));
    EXPECT_EQ(receivedPacket.payload.userName, DynamicString("user"));
    EXPECT_EQ(receivedPacket.payload.password.size, 4);
    EXPECT_EQ(receivedPacket.payload.password.data[0], 'p');
    EXPECT_EQ(receivedPacket.payload.password.data[1], 'a');
    EXPECT_EQ(receivedPacket.payload.password.data[2], 's');
    EXPECT_EQ(receivedPacket.payload.password.data[3], 's');
    EXPECT_EQ(receivedPacket.fixedVariableHeader.cleanSession, true);
    EXPECT_EQ(receivedPacket.fixedVariableHeader.keepAlive, 60);
    EXPECT_EQ(receivedPacket.payload.willMessage->topicName, DynamicString("will/topic"));
    EXPECT_EQ(receivedPacket.payload.willMessage->payload.size, 4);
    EXPECT_EQ(receivedPacket.payload.willMessage->payload.data[0], 'w');
    EXPECT_EQ(receivedPacket.payload.willMessage->payload.data[1], 'i');
    EXPECT_EQ(receivedPacket.payload.willMessage->payload.data[2], 'l');
    EXPECT_EQ(receivedPacket.payload.willMessage->payload.data[3], 'l');
    EXPECT_EQ(receivedPacket.fixedVariableHeader.willQoS, (uint8_t)QoSDelivery::AtLeastOne);
    EXPECT_EQ(receivedPacket.fixedVariableHeader.willRetain,
              static_cast<ConnectPacket*>(packet)->fixedVariableHeader.willRetain);
    EXPECT_EQ(receivedPacket.payload.willMessage->willProperties.getSerializedSize(), 0);
    EXPECT_EQ(receivedPacket.payload.willMessage->willProperties.getPropertyAt(0), nullptr);
    EXPECT_EQ(
        receivedPacket.payload.willMessage->willProperties.getProperty(PropertyId::ResponseTopic),
        nullptr);
}

TEST_F(MqttV5PacketsTests, buildPingPacket) {
    uint16_t packetID = 0;
    auto packet = PacketsBuilder::buildPingPacket();

    ASSERT_TRUE(packet->checkImpl());

    uint8_t buffer[1024] = {};
    uint32_t serializedSize = packet->serialize(buffer);
    packet->computePacketSize();
    ASSERT_GT(serializedSize, 0);
    ASSERT_LT(serializedSize, sizeof(buffer));

    PingReqPacket receivedPacket;
    uint32_t deserializableSize = receivedPacket.deserialize(buffer, serializedSize);

    EXPECT_EQ(deserializableSize, serializedSize);
    //    ASSERT_TRUE(receivedPacket.checkImpl());
}

TEST_F(MqttV5PacketsTests, buildSubscribePacket) {
    const char* topicName = "test/topic";
    const QoSDelivery qos = QoSDelivery::AtLeastOne;
    const RetainHandling retainHandling = RetainHandling::NoRetainedMessage;
    const uint16_t packetID = 0;

    auto packet = PacketsBuilder::buildSubscribePacket(packetID, topicName, retainHandling, false,
                                                       qos, true, &props);
    // packet->computePacketSize();
    ASSERT_TRUE(packet->checkImpl());

    uint8_t buffer[2058] = {};
    uint32_t serializedSize =
        static_cast<ControlPacketSerializableImpl*>(packet)->serialize(buffer);

    ASSERT_GT(serializedSize, 0);
    ASSERT_LT(serializedSize, sizeof(buffer));

    SubscribePacket receivedPacket;
    uint32_t deserializableSize = receivedPacket.deserialize(buffer, serializedSize);

    EXPECT_EQ(deserializableSize, serializedSize);
    //    ASSERT_TRUE(receivedPacket.checkImpl());
    EXPECT_EQ(receivedPacket.fixedVariableHeader.packetID, 0);
    EXPECT_EQ(receivedPacket.payload.topicList[0].topicName, DynamicString("test/topic"));
    EXPECT_EQ(receivedPacket.payload.topicList[0].QoS, (uint8_t)QoSDelivery::AtLeastOne);
    EXPECT_EQ(receivedPacket.payload.topicList[0].retainHandling,
              (uint8_t)RetainHandling::NoRetainedMessage);
    EXPECT_EQ(receivedPacket.payload.topicList[0].nonLocal, false);
    EXPECT_EQ(receivedPacket.payload.topicList[0].retainAsPublished, true);
}

TEST_F(MqttV5PacketsTests, buildandSerializePublishPacket) {
    const char* topicName = "test/topic";
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    const QoSDelivery qos = QoSDelivery::AtLeastOne;
    const bool retain = true;
    const uint16_t packetID = 1234;

    auto packet = PacketsBuilder::buildPublishPacket(packetID, topicName, payload, sizeof(payload),
                                                     qos, retain, &props);
    // packet->computePacketSize();
    ASSERT_TRUE(packet->checkImpl());

    uint8_t buffer[1024] = {};
    uint32_t serializedSize =
        static_cast<ControlPacketSerializableImpl*>(packet)->serialize(buffer);

    ASSERT_GT(serializedSize, 0);
    ASSERT_LT(serializedSize, sizeof(buffer));

    PublishPacket receivedPacket;
    uint32_t deserializableSize = receivedPacket.deserialize(buffer, serializedSize);

    EXPECT_EQ(deserializableSize, serializedSize);
    //    ASSERT_TRUE(receivedPacket.checkImpl());
    EXPECT_EQ(receivedPacket.fixedVariableHeader.topicName, DynamicString("test/topic"));
    EXPECT_EQ(receivedPacket.header.isRetained(), true);
    EXPECT_EQ(receivedPacket.header.isDuplicated(), false);
    EXPECT_EQ(receivedPacket.header.getQoS(), (uint8_t)QoSDelivery::AtLeastOne);
    EXPECT_EQ(receivedPacket.fixedVariableHeader.packetID, 1234);
    EXPECT_EQ(receivedPacket.payload.dataSize, sizeof(payload));
    EXPECT_EQ(receivedPacket.payload.data[0], 0x01);
    EXPECT_EQ(receivedPacket.payload.data[1], 0x02);
    EXPECT_EQ(receivedPacket.payload.data[2], 0x03);
    EXPECT_EQ(receivedPacket.payload.data[3], 0x04);
}
