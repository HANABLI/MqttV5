/**
 * @file MqttV5PropertiesTests.cpp
 * @brief This module contains tests for Properties chain creation,
 *      serialization and deserialisation.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */
#include <MqttV5/MqttV5Properties.hpp>
#include <MqttV5/MqttV5Constants.hpp>
#include <MqttV5/MqttV5Types.hpp>
#include <MqttV5/MqttV5ISerializable.hpp>
#include <Utf8/Utf8.hpp>
#include <gtest/gtest.h>

using namespace MqttV5;
using namespace MqttV5::Common;
using namespace MqttV5::Mqtt_V5;

TEST(MqttV5PropertiesTest, SerializeDeserialize_Uint16) {
    // ID arbitraire de propriété
    PropertyId propId = PropertyId::AssignedClientID;
    uint16_t originalValue = 0xABCD;

    // Objet valeur typé
    GenericType<uint16_t> genericValue(originalValue);

    // Création de la propriété source
    PropertyCoreImpl prop(propId, genericValue);

    // Buffer pour sérialisation
    uint8_t buffer[16] = {};
    uint32_t serializedSize = prop.serialize(buffer);

    // Vérification de la taille
    EXPECT_EQ(serializedSize, sizeof(PropertyId) + sizeof(uint16_t));

    // Vérification du contenu du buffer (BigEndian)
    EXPECT_EQ(buffer[0], propId);  // Property ID
    EXPECT_EQ(buffer[1], 0xAB);    // Big endian high byte
    EXPECT_EQ(buffer[2], 0xCD);    // Big endian low byte

    // Prépare une nouvelle valeur et propriété cible
    GenericType<uint16_t> receivedValue;
    PropertyCoreImpl prop2(propId, receivedValue);

    // Désérialisation
    uint32_t deserializedSize = prop2.deserialize(buffer, serializedSize);
    EXPECT_EQ(deserializedSize, serializedSize);

    // Vérifie que la valeur est bien restaurée
    EXPECT_EQ(static_cast<uint16_t>(receivedValue), originalValue);
}

TEST(MqttV5PropertiesTest, PropertyFactoryTest) {
    auto* prop1 = PropertyFactory<PropertyId::ReasonString, uint32_t>::create();
    auto* prop2 = PropertyFactory<PropertyId::ReasonString, uint32_t>::create(0xCAFEBABE);

    auto* typed1 = dynamic_cast<Property<uint32_t>*>(prop1);
    auto* typed2 = dynamic_cast<Property<uint32_t>*>(prop2);

    EXPECT_EQ(static_cast<uint32_t>(typed1->value), 0);
    EXPECT_EQ(static_cast<uint32_t>(typed2->value), 0xCAFEBABE);
}

TEST(MqttV5PropertiesTest, PropertyChainCreationAndValidation) {
    // registerAllProperties();

    Properties props;
    Utf8::Utf8 utf;
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
        DynamicBinaryData(reinterpret_cast<uint8_t*>(_strdup("authdata")), 8)));
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
    props.addProperty(AssignedClientIdentifier_prop::create(DynamicString("client-id")));

    EXPECT_TRUE(props.checkImpl());
    // Sérialiser
    uint8_t buffer[512] = {};
    uint32_t serializedSize = props.serialize(buffer);

    // Désérialiser dans une nouvelle instance
    Properties restored;
    uint32_t read = restored.deserialize(buffer, serializedSize);
    EXPECT_EQ(read, serializedSize);

    // Vérifier les valeurs récupérées
    auto* pf =
        dynamic_cast<Property<uint8_t>*>(restored.getProperty(PropertyId::PayloadFormatIndicator));
    auto* me =
        dynamic_cast<Property<uint32_t>*>(restored.getProperty(PropertyId::MessageExpiryInterval));
    auto* ct =
        dynamic_cast<Property<DynamicString>*>(restored.getProperty(PropertyId::ContentType));
    auto* rt =
        dynamic_cast<Property<DynamicString>*>(restored.getProperty(PropertyId::ResponseTopic));
    auto* cd = dynamic_cast<Property<DynamicBinaryData>*>(
        restored.getProperty(PropertyId::CorrelationData));
    auto* up =
        dynamic_cast<Property<DynamicStringPair>*>(restored.getProperty(PropertyId::UserProperty));
    auto* si = dynamic_cast<Property<uint32_t>*>(restored.getProperty(PropertyId::SubscriptionID));
    auto* sei =
        dynamic_cast<Property<uint32_t>*>(restored.getProperty(PropertyId::SessionExpiryInterval));
    auto* ska =
        dynamic_cast<Property<uint16_t>*>(restored.getProperty(PropertyId::ServerKeepAlive));
    auto* ri =
        dynamic_cast<Property<DynamicString>*>(restored.getProperty(PropertyId::ResponseInfo));
    auto* sr =
        dynamic_cast<Property<DynamicString>*>(restored.getProperty(PropertyId::ServerReference));
    auto* am = dynamic_cast<Property<DynamicString>*>(
        restored.getProperty(PropertyId::AuthenticationMethod));
    auto* ad = dynamic_cast<Property<DynamicBinaryData>*>(
        restored.getProperty(PropertyId::AuthenticationData));
    auto* rs =
        dynamic_cast<Property<DynamicString>*>(restored.getProperty(PropertyId::ReasonString));
    auto* ta = dynamic_cast<Property<uint16_t>*>(restored.getProperty(PropertyId::TopicAlias));
    auto* tam = dynamic_cast<Property<uint16_t>*>(restored.getProperty(PropertyId::TopicAliasMax));
    auto* mps = dynamic_cast<Property<uint32_t>*>(restored.getProperty(PropertyId::PacketSizeMax));
    auto* mq = dynamic_cast<Property<uint8_t>*>(restored.getProperty(PropertyId::QoSMax));
    auto* ra = dynamic_cast<Property<uint8_t>*>(restored.getProperty(PropertyId::RetainAvailable));
    auto* wsa =
        dynamic_cast<Property<uint8_t>*>(restored.getProperty(PropertyId::WildcardSubAvailable));
    auto* sia =
        dynamic_cast<Property<uint8_t>*>(restored.getProperty(PropertyId::SubscriptionIDAvailable));
    auto* ssa = dynamic_cast<Property<uint8_t>*>(
        restored.getProperty(PropertyId::SharedSubscriptionAvailable));
    auto* rm = dynamic_cast<Property<uint16_t>*>(restored.getProperty(PropertyId::ReceiveMax));

    auto* wdi =
        dynamic_cast<Property<uint32_t>*>(restored.getProperty(PropertyId::WillDelayInterval));

    auto* rri =
        dynamic_cast<Property<uint8_t>*>(restored.getProperty(PropertyId::RequestResponseInfo));
    auto* rpi =
        dynamic_cast<Property<uint8_t>*>(restored.getProperty(PropertyId::RequestProblemInfo));
    auto* aci =
        dynamic_cast<Property<DynamicString>*>(restored.getProperty(PropertyId::AssignedClientID));

    ASSERT_NE(pf, nullptr);
    ASSERT_NE(me, nullptr);
    ASSERT_NE(ct, nullptr);
    ASSERT_NE(rt, nullptr);
    ASSERT_NE(cd, nullptr);
    ASSERT_NE(up, nullptr);
    ASSERT_NE(si, nullptr);
    ASSERT_NE(sei, nullptr);
    ASSERT_NE(ska, nullptr);
    ASSERT_NE(ri, nullptr);
    ASSERT_NE(sr, nullptr);
    ASSERT_NE(am, nullptr);
    ASSERT_NE(ad, nullptr);
    ASSERT_NE(rs, nullptr);
    ASSERT_NE(ta, nullptr);
    ASSERT_NE(tam, nullptr);
    ASSERT_NE(mps, nullptr);
    ASSERT_NE(mq, nullptr);
    ASSERT_NE(ra, nullptr);
    ASSERT_NE(wsa, nullptr);
    ASSERT_NE(sia, nullptr);
    ASSERT_NE(ssa, nullptr);
    ASSERT_NE(rm, nullptr);
    ASSERT_NE(rri, nullptr);
    ASSERT_NE(rpi, nullptr);
    ASSERT_NE(aci, nullptr);

    auto ct_data = ct->value.data;
    std::vector<uint8_t> ct_data_vec(ct_data, ct_data + ct->value.size);
    auto decoded_ct = utf.Decode(ct_data_vec);
    auto rt_data = rt->value.data;
    std::vector<uint8_t> rt_data_vec(rt_data, rt_data + rt->value.size);
    auto decoded_rt = utf.Decode(rt_data_vec);
    auto cd_data = cd->value.data;
    std::vector<uint8_t> cd_data_vec(cd_data, cd_data + cd->value.size);
    auto decoded_cd = utf.Decode(cd_data_vec);
    //
    auto up_first_data = static_cast<DynamicStringPair&>(up->value).first.data;
    std::vector<uint8_t> up_first_data_vec(up_first_data, up_first_data + up->value.first.size);
    auto decoded_up_first = utf.Decode(up_first_data_vec);

    auto up_second_data = static_cast<DynamicStringPair&>(up->value).second.data;
    std::vector<uint8_t> up_second_data_vec(up_second_data, up_second_data + up->value.second.size);
    auto decoded_up_second = utf.Decode(up_second_data_vec);

    EXPECT_EQ(static_cast<uint8_t>(pf->value), 0x01);
    EXPECT_EQ(static_cast<uint32_t>(me->value), 120000U);
    EXPECT_EQ(decoded_ct, Utf8::AsciiToUnicode("application/json"));
    EXPECT_EQ(decoded_rt, Utf8::AsciiToUnicode("response/topic"));
    EXPECT_EQ(static_cast<DynamicBinaryData&>(cd->value).size, 4);
    EXPECT_EQ(decoded_cd, Utf8::AsciiToUnicode("ABCD"));
    EXPECT_EQ(decoded_up_first, Utf8::AsciiToUnicode("key"));
    EXPECT_EQ(decoded_up_second, Utf8::AsciiToUnicode("value"));
}

TEST(MqttV5PropertiesTests, SerializeAndDeserialize) {
    // Create a property
    // Objet valeur typé
    uint8_t originalValue = 0xAB;
    PropertyCore* propertie = PayloadFormatIndicator_prop::create(originalValue);

    // Serialize the property
    uint8_t buffer[8] = {};
    uint32_t serializedSize = propertie->serialize(buffer);

    // Deserialize the property
    MqttV5::PropertyCore* deserializedProperty = PayloadFormatIndicator_prop::create();
    uint32_t deserializedSize = deserializedProperty->deserialize(buffer, serializedSize);

    // Verify the deserialized property
    EXPECT_EQ(deserializedProperty->id, MqttV5::PropertyId::PayloadFormatIndicator);
    Property<uint8_t>* typedPropertySource = dynamic_cast<Property<uint8_t>*>(propertie);
    Property<uint8_t>* typedPropertyDest = dynamic_cast<Property<uint8_t>*>(deserializedProperty);
    EXPECT_EQ(typedPropertySource->value.value, typedPropertyDest->value.value);

    delete deserializedProperty;
    delete propertie;
}

TEST(MqttV5PropertiesTests, SerializeAndDeserializeWithDynamicString) {
    // Create a property with a dynamic string
    PropertyCore* property = ContentType_prop::create("application/json");

    // Serialize the property
    uint8_t buffer[256] = {};
    uint32_t serializedSize = property->serialize(buffer);

    // Deserialize the property
    PropertyCore* deserializedProperty = ContentType_prop::create();
    deserializedProperty->deserialize(buffer, serializedSize);

    // Verify the deserialized property
    EXPECT_EQ(deserializedProperty->id, MqttV5::PropertyId::ContentType);
    // EXPECT_STREQ(reinterpret_cast<const char*>(deserializedProperty->value.data),
    //              "application/json");

    delete deserializedProperty;
    delete property;
}

TEST(MqttV5PropertiesTests, SerializeAndDeserializeWithDynamicBinaryData) {
    // Create a property with dynamic binary data
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    MqttV5::PropertyCore* property = CorrelationData_prop::create(DynamicBinaryData(data, 4));

    // Serialize the property
    uint8_t buffer[256] = {};
    uint32_t serializedSize = property->serialize(buffer);

    // Deserialize the property
    MqttV5::PropertyCore* deserializedProperty = CorrelationData_prop::create();
    deserializedProperty->deserialize(buffer, serializedSize);

    // Verify the deserialized property
    EXPECT_EQ(deserializedProperty->id, MqttV5::PropertyId::CorrelationData);
    EXPECT_EQ(dynamic_cast<Property<DynamicBinaryData>*>(deserializedProperty)->value.size,
              sizeof(data));

    delete deserializedProperty;
}

// TEST(MqttV5PropertiesTests, ThreadSafetyInDeserializationRegistry) {
//     DeserializationRegistry& registry = DeserializationRegistry::getInstance();

//     // Register a property in a thread-safe manner
//     std::thread t1(
//         [&]()
//         {
//             registry.registerDeserializer(
//                 MqttV5::PropertyId::PayloadFormatIndicator,
//                 [](const uint8_t* buffer, size_t bufferSize) -> MqttV5::PropertyCore* {
//                     return new
//                     MqttV5::PropertyBase(MqttV5::PropertyId::PayloadFormatIndicator);
//                 });
//         });

//     std::thread t2(
//         [&]()
//         {
//             registry.registerDeserializer(
//                 MqttV5::PropertyId::MessageExpiryInterval,
//                 [](const uint8_t* buffer, size_t bufferSize) -> MqttV5::PropertyCore* {
//                     return new
//                     MqttV5::PropertyBase(MqttV5::PropertyId::MessageExpiryInterval);
//                 });
//         });

//     t1.join();
//     t2.join();

//     // Verify that both properties are registered
//     uint8_t buffer[1] = {0};
//     EXPECT_NE(
//         registry.deserialize(MqttV5::PropertyId::PayloadFormatIndicator, buffer,
//         sizeof(buffer)), nullptr);
//     EXPECT_NE(
//         registry.deserialize(MqttV5::PropertyId::MessageExpiryInterval, buffer,
//         sizeof(buffer)), nullptr);
// }
