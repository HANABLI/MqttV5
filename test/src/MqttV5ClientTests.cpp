/**
 * @file MqttV5ClientTests.cpp
 * @brief This file contains the implementation of the MqttClientTest class.
 * * @note This class is used to test the MqttClient Methods.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */

#include <gtest/gtest.h>
#include <atomic>
#include <functional>
#include <memory>
#include <future>
#include <string>
#include <array>
#include <vector>
#include <cstdint>
#include <cassert>
#include "MqttV5/MqttClient.hpp"
#include "MqttV5/MqttV5Packets.hpp"
#include <StringUtils/StringUtils.hpp>
#include "MqttV5/ClientTransportLayer.hpp"
#include "MqttV5/MqttV5Properties.hpp"
#include "MqttV5/Connection.hpp"
#include "MqttV5/TimeKeeper.hpp"
#include "SystemUtils/DiagnosticsSender.hpp"

using namespace MqttV5;
using namespace MqttV5::Common;
using namespace MqttV5::Mqtt_V5;

namespace
{
    struct MockConnection : public Connection
    {
        /**
         * This is used to synchronize access to the wait condition.
         */
        std::recursive_mutex mutex;

        /**
         * This is used to wait for, or signal, a condition
         * upon which that the tests might be waiting.
         */
        std::condition_variable_any waitCondition;

        /**
         * This is the sheme indicated in the URI of the target of the connection.
         */
        std::string scheme;

        /**
         * This is the host name or IP address of the target brocker.
         */
        std::string hostNameOrIpAddress;

        /**
         * This is the port number of the mocker target brocker.
         */
        uint16_t port = 1883;

        /**
         * This buffer is used to reassemble fragmmented mqtt requests
         * received from the Brocker.
         */
        std::vector<std::vector<uint8_t>> outgoing;

        /**
         * This is the delegate to call whenever data is received from the
         * brocker
         */
        DataReceivedDelegate dataReceivedDelegate;

        /**
         * This is the delegate to call whenever the connection has been brocken.
         */
        BrokenDelegate brokenDelegate;

        /**
         * This flag indicate whether the brocker break the connection.
         */
        bool broken = false;

        std::string GetPeerId() override {
            return StringUtils::sprintf("%s:%" PRIu16, hostNameOrIpAddress.c_str(), port);
        }

        void SetDataReceivedDelegate(DataReceivedDelegate newDataReceivedDelegate) override {
            dataReceivedDelegate = newDataReceivedDelegate;
        }

        void SetConnectionBrokenDelegate(BrokenDelegate newBrokenDelegate) override {
            brokenDelegate = newBrokenDelegate;
        }

        void SendData(const std::vector<uint8_t>& data) override { outgoing.push_back(data); }

        void Break(bool clean) override {
            if (brokenDelegate)
                brokenDelegate(clean);
        }

        void SimulateIncoming(const std::vector<uint8_t>& bytes) {
            if (dataReceivedDelegate)
                dataReceivedDelegate(bytes);
        }

        void SimulateBroken(bool clean = false) { Break(clean); }

        const std::vector<uint8_t> LastOutgoing() {
            assert(!outgoing.empty());
            const auto back = outgoing.back();
            outgoing.pop_back();
            return back;
        }
    };

    struct MockTransport : public ClientTransportLayer
    {
        /**
         * This is used to synchronize access to the wait condition.
         */
        std::recursive_mutex mutex;

        /**
         * This is used to wait for, or signal, a condition
         * upon which that the tests might be waiting.
         */
        std::condition_variable_any waitCondition;

        /**
         * This is the connection object created by the client to reach the brocker.
         */
        std::shared_ptr<MockConnection> lastConn;

        std::shared_ptr<Connection> Connect(const std::string& scheme,
                                            const std::string& hostNameOrAddress, uint16_t port,
                                            Connection::DataReceivedDelegate dataCb,
                                            Connection::BrokenDelegate brokenCb) override {
            lastConn = std::make_shared<MockConnection>();
            lastConn->scheme = scheme;
            lastConn->hostNameOrIpAddress = hostNameOrAddress;
            lastConn->port = port;
            lastConn->SetDataReceivedDelegate(std::move(dataCb));
            lastConn->SetConnectionBrokenDelegate(std::move(brokenCb));
            return lastConn;
        }
    };

    struct MockTimeKeeper : public MqttV5::TimeKeeper
    {  // Properties

        double currentTime = 0.0;

        // Methods

        // Mqtt::TimeKeeper

        virtual double GetCurrentTime() override { return currentTime; }
    };

    struct OptionImpl : public MqttClient::MqttOptions
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
         * This is only recommanded if you are sure about your broker implementation (don't set
         * this to 1 if you intend to connect to unknown broker) Default: 0
         * */
        bool avoidValidation = false;
        /**
         * Set to:
         * - 1 for enable complete QoS management. This imply allocating and using a buffer
         * that's 3x the maximum packet size you've set (for saving QoS packets against
         * disconnection) plus a small overhead in binary size.
         * - 0 to implement a non compliant, but QoS capable client. No packet are saved for
         * QoS, but the client will claim and follow QoS protocols for publish cycle. The binary
         * overhead is also reduced.
         * - -1 to disable QoS management. The client will never claim it is QoS capable. Saves
         * the maximum binary size. Default: 1
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

    // App-level callback receiver
    struct AppReceiver : public Storage::MessageReceived
    {
        std::vector<std::string> receivedTopics;
        std::vector<std::string> receivedPayloads;
        std::atomic<int> lost{0};

        void onMessageReceived(const Storage::DynamicStringView& topic,
                               Storage::DynamicBinaryDataView& payload, uint16_t /*packetId*/,
                               Storage::Properties& /*properties*/) override {
            receivedTopics.emplace_back(std::string(topic.data, topic.size));
            receivedPayloads.emplace_back(
                std::string(reinterpret_cast<const char*>(payload.data), payload.size));
        }

        bool onConnectionLost(
            const MqttV5::IMqttV5Client::Transaction::State& state /*reason*/) override {
            ++lost;
            return true;
        }

        // caps
        uint32_t maxPacketSize() const override { return 4096; }
        uint32_t maxUnAckedPackets() const override { return 8; }
    };

}  // namespace

struct MqttV5ClientTests : public ::testing::Test
{
    static Properties props;
    Utf8::Utf8 utf;

protected:
    OptionImpl options;

    std::unique_ptr<MockTransport> transport;

    std::unique_ptr<MockTimeKeeper> time;

    std::unique_ptr<AppReceiver> app;

    std::unique_ptr<MqttV5::Storage::PacketStore> store;  // optional

    std::unique_ptr<MqttV5::MqttClient> client;

    std::vector<std::string> diagnosticMessages;

    SystemUtils::DiagnosticsSender::UnsubscribeDelegate diagnosticUnsubscribeDelegate;

    void SetUp() override {
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

        transport = std::make_unique<MockTransport>();
        time = std::make_unique<MockTimeKeeper>();
        app = std::make_unique<AppReceiver>();
        store = nullptr;

        client =
            std::make_unique<MqttV5::MqttClient>("test-Client", app.get(), nullptr, store.get());

        MqttV5::MqttClient::MqttMobilizationDependencies deps;
        deps.transport =
            std::shared_ptr<MqttV5::ClientTransportLayer>(transport.get(), [](auto*) {});
        deps.timeKeeper = std::shared_ptr<MqttV5::TimeKeeper>(time.get(), [](auto*) {});
        deps.requestTimeoutSeconds = 5.0;
        deps.inactivityInterval = 120.0;
        deps.options = options;
        client->Mobilize(deps);
        diagnosticUnsubscribeDelegate = client->SubscribeToDiagnostics(
            [this](std::string senderName, size_t level, std::string message)
            {
                diagnosticMessages.push_back(StringUtils::sprintf("%s[%zu]: %s", senderName.c_str(),
                                                                  level, message.c_str()));
            },
            0);
    }

    void TearDown() override {
        // Clean up any resources allocated during the tests
        client->Demobilize();

        diagnosticUnsubscribeDelegate();

        props.clear();
    }

    MockConnection* conn() const { return transport->lastConn.get(); }
};

Properties MqttV5ClientTests::props;

TEST_F(MqttV5ClientTests, SimpleConnectThenConnAckOk_test) {
    std::string clientId = "client";
    std::string userName = "user";
    std::string string_pass = "pass";
    std::string willPayload = "online";
    auto encodedPass = utf.Encode(Utf8::AsciiToUnicode(string_pass));
    DynamicBinaryData password(encodedPass.data(), static_cast<uint32_t>(encodedPass.size()));
    auto encodedWill = utf.Encode(Utf8::AsciiToUnicode(willPayload));
    DynamicBinaryData will(encodedWill.data(), static_cast<uint32_t>(encodedWill.size()));

    WillMessage willMsg;
    willMsg.topicName = "will/topic";
    willMsg.payload = will;
    auto transaction = client->ConnectTo("broker.test", 1883, false, true, 60, nullptr, nullptr,
                                         &willMsg, MqttV5::QoSDelivery::AtLeastOne, false, &props);
    ASSERT_NE(conn(), nullptr) << "Trasport Layer don't create the connection object";

    const auto& connection = conn();
    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult, transaction->transactionState);
    ASSERT_FALSE(connection->outgoing.empty());
    const auto connAkt = connection->LastOutgoing();
    ConnectPacket packet;
    auto desSize = packet.deserialize(connAkt.data(), (uint32_t)connAkt.size());
    ASSERT_EQ(ControlPacketType::CONNECT, packet.header.getType());

    auto connAckPack = PacketsBuilder::buildConnAckPacket(ReasonCode::Success, &props);
    uint8_t buffer[256] = {};
    auto packetSize = connAckPack->serialize(buffer);
    std::vector<uint8_t> data(buffer, buffer + packetSize);

    std::promise<void> transactionCompleted;
    transaction->SetCompletionDelegate(
        [&transactionCompleted](std::vector<Storage::ReasonCode>& reasons)
        {
            transactionCompleted.set_value();
            for (const Storage::ReasonCode& i : reasons)
            { EXPECT_EQ(Storage::ReasonCode::Success, i); }
        });
    connection->SimulateIncoming({data.begin(), data.end()});

    auto transactionWasCompleted = transactionCompleted.get_future();
    ASSERT_EQ(std::future_status::ready, transactionWasCompleted.wait_for(std::chrono::seconds(1)));
    EXPECT_EQ(MqttClient::Transaction::State::Success, transaction->transactionState);
}

TEST_F(MqttV5ClientTests, SubscribeSingleTopicSendsPacket_test) {
    std::string clientId = "client";
    std::string userName = "user";
    std::string string_pass = "pass";
    std::string willPayload = "online";
    auto encodedPass = utf.Encode(Utf8::AsciiToUnicode(string_pass));
    DynamicBinaryData password(encodedPass.data(), static_cast<uint32_t>(encodedPass.size()));
    auto encodedWill = utf.Encode(Utf8::AsciiToUnicode(willPayload));
    DynamicBinaryData will(encodedWill.data(), static_cast<uint32_t>(encodedWill.size()));

    WillMessage willMsg;
    willMsg.topicName = "will/topic";
    willMsg.payload = will;

    auto transaction = client->ConnectTo("broker.test", 1883, false, true, 60, nullptr, nullptr,
                                         &willMsg, MqttV5::QoSDelivery::AtLeastOne, false, &props);
    ASSERT_NE(conn(), nullptr) << "Transport Layer dont create the connection object";
    const auto& connection = conn();
    // std::weak_ptr<MqttClient::Transaction> weakTransaction = transaction;
    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult, transaction->transactionState);
    ASSERT_FALSE(connection->outgoing.empty());
    const auto connAkt = connection->LastOutgoing();
    ConnectPacket packet;
    auto desSize = packet.deserialize(connAkt.data(), (uint32_t)connAkt.size());
    ASSERT_EQ(ControlPacketType::CONNECT, packet.header.getType());

    auto connAckPack = PacketsBuilder::buildConnAckPacket(ReasonCode::Success, &props);
    uint8_t buffer[256] = {};
    auto packetSize = connAckPack->serialize(buffer);
    std::vector<uint8_t> data(buffer, buffer + packetSize);

    std::promise<void> transactionCompleted;
    transaction->SetCompletionDelegate(
        [&transactionCompleted](std::vector<Storage::ReasonCode>& reasons)
        {
            transactionCompleted.set_value();
            for (const Storage::ReasonCode& i : reasons)
            { EXPECT_EQ(Storage::ReasonCode::Success, i); }
        });
    connection->SimulateIncoming({data.begin(), data.end()});

    auto transactionWasCompleted = transactionCompleted.get_future();
    ASSERT_EQ(std::future_status::ready, transactionWasCompleted.wait_for(std::chrono::seconds(1)));

    EXPECT_EQ(MqttClient::Transaction::State::Success, transaction->transactionState);

    auto subTransaction = client->Subscribe("sensors/+/temp", RetainHandling::NoRetainedMessage,
                                            false, MqttV5::QoSDelivery::AtLeastOne, true, &props);

    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult, subTransaction->transactionState);
    ASSERT_FALSE(connection->outgoing.empty());
    const auto subPkt = connection->LastOutgoing();
    SubscribePacket subpack;
    (void)subpack.deserialize(subPkt.data(), (uint32_t)subPkt.size());
    ASSERT_EQ(MqttV5::ControlPacketType::SUBSCRIBE, subpack.header.getType());
    ASSERT_EQ(subTransaction->packetID, subpack.fixedVariableHeader.packetID);
}

TEST_F(MqttV5ClientTests, SubscribeSubAckSuccessGarantedQos1_test) {
    std::string clientId = "client";
    std::string userName = "user";
    std::string string_pass = "pass";
    std::string willPayload = "online";
    auto encodedPass = utf.Encode(Utf8::AsciiToUnicode(string_pass));
    DynamicBinaryData password(encodedPass.data(), static_cast<uint32_t>(encodedPass.size()));
    auto encodedWill = utf.Encode(Utf8::AsciiToUnicode(willPayload));
    DynamicBinaryData will(encodedWill.data(), static_cast<uint32_t>(encodedWill.size()));

    WillMessage willMsg;
    willMsg.topicName = "will/topic";
    willMsg.payload = will;

    auto transaction = client->ConnectTo("broker.test", 1883, false, true, 60, nullptr, nullptr,
                                         &willMsg, MqttV5::QoSDelivery::AtLeastOne, false, &props);
    ASSERT_NE(conn(), nullptr) << "Transport Layer dont create the connection object";
    const auto& connection = conn();
    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult, transaction->transactionState);
    auto connAckPack = PacketsBuilder::buildConnAckPacket(ReasonCode::Success, &props);
    uint8_t buffer[256] = {};
    auto packetSize = connAckPack->serialize(buffer);
    std::vector<uint8_t> data(buffer, buffer + packetSize);

    std::promise<void> transactionCompleted;
    transaction->SetCompletionDelegate(
        [&transactionCompleted](std::vector<Storage::ReasonCode>& reasons)
        {
            transactionCompleted.set_value();
            for (const Storage::ReasonCode& i : reasons)
            { EXPECT_EQ(Storage::ReasonCode::Success, i); }
        });

    connection->SimulateIncoming({data.begin(), data.end()});

    auto transactionWasCompleted = transactionCompleted.get_future();
    ASSERT_EQ(std::future_status::ready, transactionWasCompleted.wait_for(std::chrono::seconds(1)));
    EXPECT_EQ(MqttClient::Transaction::State::Success, transaction->transactionState);

    auto subTransaction = client->Subscribe("sensors/+/temp", RetainHandling::NoRetainedMessage,
                                            false, MqttV5::QoSDelivery::AtLeastOne, true, &props);
    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult, subTransaction->transactionState);

    auto subAckPack = PacketsBuilder::buildSubAckPacket(1, ReasonCode::GrantedQoS1, &props);
    uint8_t subAckBuffer[256] = {};
    auto subPackSize = subAckPack->serialize(subAckBuffer);
    std::vector<uint8_t> subAckIncomData(subAckBuffer, subAckBuffer + subPackSize);
    std::promise<void> subAcktransactionCompleted;
    subTransaction->SetCompletionDelegate(
        [&subAcktransactionCompleted](std::vector<Storage::ReasonCode>& reasons)
        {
            subAcktransactionCompleted.set_value();
            for (const Storage::ReasonCode& i : reasons)
            { EXPECT_EQ(Storage::ReasonCode::GrantedQoS1, i); }
        });

    connection->SimulateIncoming({subAckIncomData.begin(), subAckIncomData.end()});

    transactionWasCompleted = subAcktransactionCompleted.get_future();
    ASSERT_EQ(std::future_status::ready, transactionWasCompleted.wait_for(std::chrono::seconds(1)));
    EXPECT_EQ(MqttClient::Transaction::State::Success, subTransaction->transactionState);
}

TEST_F(MqttV5ClientTests, SubscribeSubAckFailureNotAuthorized_test) {
    std::string clientId = "client";
    std::string userName = "user";
    std::string string_pass = "pass";
    std::string willPayload = "online";
    auto encodedPass = utf.Encode(Utf8::AsciiToUnicode(string_pass));
    DynamicBinaryData password(encodedPass.data(), static_cast<uint32_t>(encodedPass.size()));
    auto encodedWill = utf.Encode(Utf8::AsciiToUnicode(willPayload));
    DynamicBinaryData will(encodedWill.data(), static_cast<uint32_t>(encodedWill.size()));

    WillMessage willMsg;
    willMsg.topicName = "will/topic";
    willMsg.payload = will;

    auto transaction = client->ConnectTo("broker.test", 1883, false, true, 60, nullptr, nullptr,
                                         &willMsg, MqttV5::QoSDelivery::AtLeastOne, false, &props);
    ASSERT_NE(conn(), nullptr) << "Transport Layer dont create the connection object";
    const auto& connection = conn();
    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult, transaction->transactionState);
    auto connAckPack = PacketsBuilder::buildConnAckPacket(ReasonCode::Success, &props);
    uint8_t buffer[256] = {};
    auto packetSize = connAckPack->serialize(buffer);
    std::vector<uint8_t> data(buffer, buffer + packetSize);

    std::promise<void> transactionCompleted;
    transaction->SetCompletionDelegate(
        [&transactionCompleted](std::vector<Storage::ReasonCode>& reasons)
        {
            transactionCompleted.set_value();
            for (const Storage::ReasonCode& i : reasons)
            { EXPECT_EQ(Storage::ReasonCode::Success, i); }
        });

    connection->SimulateIncoming({data.begin(), data.end()});

    auto transactionWasCompleted = transactionCompleted.get_future();
    ASSERT_EQ(std::future_status::ready, transactionWasCompleted.wait_for(std::chrono::seconds(1)));
    EXPECT_EQ(MqttClient::Transaction::State::Success, transaction->transactionState);

    auto subTransaction = client->Subscribe("sensors/+/temp", RetainHandling::NoRetainedMessage,
                                            false, MqttV5::QoSDelivery::AtLeastOne, true, &props);
    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult, subTransaction->transactionState);

    auto subAckPack = PacketsBuilder::buildSubAckPacket(1, ReasonCode::NotAuthorized, &props);
    uint8_t subAckBuffer[256] = {};
    auto subPackSize = subAckPack->serialize(subAckBuffer);
    std::vector<uint8_t> subAckIncomData(subAckBuffer, subAckBuffer + subPackSize);
    std::promise<void> subAcktransactionCompleted;
    subTransaction->SetCompletionDelegate(
        [&subAcktransactionCompleted](std::vector<Storage::ReasonCode>& reasons)
        {
            subAcktransactionCompleted.set_value();
            for (const Storage::ReasonCode& i : reasons)
            { EXPECT_EQ(Storage::ReasonCode::NotAuthorized, i); }
        });

    connection->SimulateIncoming({subAckIncomData.begin(), subAckIncomData.end()});

    transactionWasCompleted = subAcktransactionCompleted.get_future();
    ASSERT_EQ(std::future_status::ready, transactionWasCompleted.wait_for(std::chrono::seconds(1)));
    EXPECT_EQ(MqttClient::Transaction::State::NetworkError, subTransaction->transactionState);
}

TEST_F(MqttV5ClientTests, SubscribeMultiTopicsList_test) {
    std::string clientId = "client";
    std::string userName = "user";
    std::string string_pass = "pass";
    std::string willPayload = "online";
    auto encodedPass = utf.Encode(Utf8::AsciiToUnicode(string_pass));
    DynamicBinaryData password(encodedPass.data(), static_cast<uint32_t>(encodedPass.size()));
    auto encodedWill = utf.Encode(Utf8::AsciiToUnicode(willPayload));
    DynamicBinaryData will(encodedWill.data(), static_cast<uint32_t>(encodedWill.size()));

    WillMessage willMsg;
    willMsg.topicName = "will/topic";
    willMsg.payload = will;

    auto transaction = client->ConnectTo("broker.test", 1883, false, true, 60, nullptr, nullptr,
                                         &willMsg, MqttV5::QoSDelivery::AtLeastOne, false, &props);
    ASSERT_NE(conn(), nullptr) << "Transport Layer dont create the connection object";
    const auto& connection = conn();
    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult, transaction->transactionState);
    auto connAckPack = PacketsBuilder::buildConnAckPacket(ReasonCode::Success, &props);
    uint8_t buffer[256] = {};
    auto packetSize = connAckPack->serialize(buffer);
    std::vector<uint8_t> data(buffer, buffer + packetSize);

    std::promise<void> transactionCompleted;
    transaction->SetCompletionDelegate(
        [&transactionCompleted](std::vector<Storage::ReasonCode>& reasons)
        {
            transactionCompleted.set_value();
            for (const Storage::ReasonCode& i : reasons)
            { EXPECT_EQ(Storage::ReasonCode::Success, i); }
        });

    connection->SimulateIncoming({data.begin(), data.end()});

    auto transactionWasCompleted = transactionCompleted.get_future();
    ASSERT_EQ(std::future_status::ready, transactionWasCompleted.wait_for(std::chrono::seconds(1)));
    EXPECT_EQ(MqttClient::Transaction::State::Success, transaction->transactionState);

    auto topic1 = new SubscribeTopic("sensors/+/temp", RetainHandling::NoRetainedMessage, false,
                                     true, MqttV5::QoSDelivery::AtLeastOne);
    auto topic2 = new SubscribeTopic("sensors/+/humidity", RetainHandling::NoRetainedMessage, false,
                                     true, MqttV5::QoSDelivery::AtLeastOne);

    topic1->append(topic2);
    auto multiSubTransactions = client->Subscribe(topic1, &props);

    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult,
              multiSubTransactions->transactionState);
    const std::vector<uint8_t> reasons = {ReasonCode::GrantedQoS1, ReasonCode::GrantedQoS1};
    auto subAckPack = PacketsBuilder::buildSubAckPacketMultiTopics(1, reasons, &props);
    uint8_t subAckBuffer[256] = {};
    auto subPackSize = subAckPack->serialize(subAckBuffer);
    std::vector<uint8_t> subAckIncomData(subAckBuffer, subAckBuffer + subPackSize);
    std::promise<void> subAcktransactionCompleted;
    multiSubTransactions->SetCompletionDelegate(
        [&subAcktransactionCompleted](std::vector<Storage::ReasonCode>& reasons)
        {
            subAcktransactionCompleted.set_value();
            for (const Storage::ReasonCode& i : reasons)
            { EXPECT_EQ(Storage::ReasonCode::GrantedQoS1, i); }
        });

    connection->SimulateIncoming({subAckIncomData.begin(), subAckIncomData.end()});

    transactionWasCompleted = subAcktransactionCompleted.get_future();
    ASSERT_EQ(std::future_status::ready, transactionWasCompleted.wait_for(std::chrono::seconds(1)));
    EXPECT_EQ(MqttClient::Transaction::State::Success, multiSubTransactions->transactionState);
}

TEST_F(MqttV5ClientTests, UnsubscribeThenUnsubAck_test) {
    std::string clientId = "client";
    std::string userName = "user";
    std::string string_pass = "pass";
    std::string willPayload = "online";
    auto encodedPass = utf.Encode(Utf8::AsciiToUnicode(string_pass));
    DynamicBinaryData password(encodedPass.data(), static_cast<uint32_t>(encodedPass.size()));
    auto encodedWill = utf.Encode(Utf8::AsciiToUnicode(willPayload));
    DynamicBinaryData will(encodedWill.data(), static_cast<uint32_t>(encodedWill.size()));

    WillMessage willMsg;
    willMsg.topicName = "will/topic";
    willMsg.payload = will;
    auto transaction = client->ConnectTo("broker.test", 1883, false, true, 60, nullptr, nullptr,
                                         &willMsg, MqttV5::QoSDelivery::AtLeastOne, false, &props);
    ASSERT_NE(conn(), nullptr) << "Transport Layer dont create the connection object";
    const auto& connection = conn();
    auto topic1 = new UnsubscribeTopic("sensors/+/temp");
    auto unsubscribeTransaction = client->Unsubscribe(topic1, &props);
    ASSERT_EQ(MqttClient::Transaction::State::WaitingForResult,
              unsubscribeTransaction->transactionState);
    auto unsubscribePacket =
        MqttV5::PacketsBuilder::buildUnsubAckPacket(1, {ReasonCode::Success}, &props);
    auto unsubPacketSize = unsubscribePacket->computePacketSize(true);
    auto subAckBuffer = new uint8_t[(size_t)unsubPacketSize];
    auto subPackSize = unsubscribePacket->serialize(subAckBuffer);
    std::vector<uint8_t> subAckIncomData(subAckBuffer, subAckBuffer + subPackSize);
    delete[] subAckBuffer;
    std::promise<void> unSubAcktransactionCompleted;
    unsubscribeTransaction->SetCompletionDelegate(
        [&unSubAcktransactionCompleted](std::vector<Storage::ReasonCode>& reasons)
        {
            unSubAcktransactionCompleted.set_value();
            for (const Storage::ReasonCode& i : reasons)
            { EXPECT_EQ(Storage::ReasonCode::Success, i); }
        });

    connection->SimulateIncoming({subAckIncomData.begin(), subAckIncomData.end()});

    auto transactionWasCompleted = unSubAcktransactionCompleted.get_future();
    ASSERT_EQ(std::future_status::ready, transactionWasCompleted.wait_for(std::chrono::seconds(1)));
    EXPECT_EQ(MqttClient::Transaction::State::Success, unsubscribeTransaction->transactionState);
}