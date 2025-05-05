/**
 * @file MqttV5TypesTests.cpp
 * @brief This file contains the tests for the MqttV5Types class.
 * @author Hatem Nabli
 * copyright © 2025 by Hatem Nabli
 */
#include <gtest/gtest.h>
#include <MqttV5/MqttV5Types.hpp>
using namespace MqttV5::Common;
using namespace MqttV5;

TEST(MqttV5TypesTest, GenericTypeTest) {
    using namespace Common;

    GenericType<uint16_t> gt(0x1234);
    EXPECT_EQ(gt.typeSize(), sizeof(uint16_t));
    EXPECT_EQ(gt.value, 0x1234);

    gt.swapNetworkOrder();
    EXPECT_EQ(gt.value, 0x3412);  // Check endian swap

    uint16_t* raw = static_cast<uint16_t*>(gt.raw());
    EXPECT_EQ(*raw, 0x3412);
}
TEST(MqttV5TypesTest, VBIntOperatorUint32) {
    using namespace Common;

    // Cas normal : valeur valide
    VBInt vbint;
    vbint = 0x123456;                      // Affecte une valeur
    EXPECT_EQ(uint32_t(vbint), 0x123456);  // Vérifie la conversion

    // Cas limite : valeur minimale
    vbint = 0x0;
    EXPECT_EQ((uint32_t)vbint, 0x0);  // Vérifie la conversion pour 0

    // Cas limite : valeur maximale sur 4 octets
    vbint = 0x0FFFFFFF;
    EXPECT_EQ(uint32_t(vbint), 0x0FFFFFFF);  // Vérifie la conversion pour la valeur maximale

    // Cas invalide : taille 0
    vbint.size = 0;
    EXPECT_EQ((uint32_t)vbint, 0);  // Vérifie que la conversion retourne 0

    // Cas invalide : taille > 4
    vbint.size = 5;
    EXPECT_EQ((uint32_t)vbint, 0);  // Vérifie que la conversion retourne 0
}

TEST(MqttV5TypesTest, VBIntTest) {
    using namespace Common;

    VBInt vbint;
    vbint = 0x123456;
    EXPECT_EQ(vbint.size, 3);
    EXPECT_EQ(uint32_t(vbint), 0x123456);

    uint8_t buffer[4];
    uint32_t serializedSize = vbint.serialize(buffer);
    EXPECT_EQ(serializedSize, vbint.getSerializedSize());
    ASSERT_TRUE(vbint.checkImpl());

    VBInt vbint2;
    uint32_t deserializedSize = vbint2.deserialize(buffer, serializedSize);
    EXPECT_EQ(deserializedSize, vbint.getSerializedSize());
    EXPECT_EQ(vbint2, vbint);
}

TEST(MqttV5TypesTest, DynamicStringTest) {
    using namespace Common;
    DynamicString str("Hello");
    EXPECT_EQ(str.size, 5);
    EXPECT_EQ(std::memcmp(str.data, "Hello", 5), 0);

    uint8_t buffer[10];
    uint32_t serializedSize = str.serialize(buffer);
    EXPECT_EQ(str.getSerializedSize(), 7);

    DynamicString str2;
    uint32_t deserializedSize = str2.deserialize(buffer, serializedSize);
    EXPECT_EQ(deserializedSize, 7);
    EXPECT_EQ(str2.size, str.size);  // Vérifie que les tailles sont identiques
    EXPECT_EQ(std::memcmp(str2.data, str.data, str.size), 0);  // Compare les contenus des données
}

TEST(MqttV5TypesTest, DynamicStringPairTest) {
    using namespace Common;

    DynamicStringPair pair("Key", "Value");
    EXPECT_EQ(pair.first.size, 3);
    EXPECT_EQ(pair.second.size, 5);

    uint8_t buffer[20];
    uint32_t serializedSize = pair.serialize(buffer);
    EXPECT_EQ(pair.getSerializedSize(), serializedSize);
    EXPECT_EQ(serializedSize, 12);

    DynamicStringPair pair2;
    uint32_t desrializedSize = pair2.deserialize(buffer, serializedSize);
    EXPECT_EQ(desrializedSize, 12);
    EXPECT_EQ(pair2.first.getSerializedSize(), 5);
    EXPECT_EQ(pair2.second.getSerializedSize(), 7);
    EXPECT_EQ(std::memcmp(pair2.first.data, "Key", 3), 0);
    EXPECT_EQ(std::memcmp(pair2.second.data, "Value", 5), 0);
}

TEST(MqttV5TypesTests, DynamicStringViewTest) {
    using namespace Common;
    std::string_view hello = "Hello";
    DynamicStringView view(hello);
    EXPECT_EQ(view.size, 5);
    uint8_t buffer[10];
    uint32_t serializedSize = view.serialize(buffer);
    EXPECT_EQ(serializedSize, 7);
    EXPECT_EQ(serializedSize, view.getSerializedSize());

    DynamicStringView view2;
    uint32_t deserializedSize = view2.deserialize(buffer, serializedSize);
    EXPECT_EQ(deserializedSize, view.getSerializedSize());
    EXPECT_EQ(std::memcmp(view2.data, view.data, view.size),
              0);  // Compare les contenus des données
}

TEST(MqttV5TypesTest, FixedHeaderTest) {
    using namespace Mqtt_V5;

    FixedHeader header;
    header.raw = 0x92;  // Example raw value
    EXPECT_EQ(header.type, 0x9);
    EXPECT_EQ(header.qos, 0x1);
    EXPECT_EQ(header.retain, 0x0);
}

TEST(MqttV5TypesTest, FixedHeaderTypeTest) {
    using namespace Mqtt_V5;

    ConnectFixedHeader connectHeader;
    EXPECT_EQ(connectHeader.getType(), CONNECT);
    EXPECT_EQ(connectHeader.getFlags(), 0);

    PublishFixedHeader publishHeader(0x0A);
    EXPECT_EQ(publishHeader.getType(), PUBLISH);
    EXPECT_EQ(publishHeader.getFlags(), 0x0A);
}

TEST(MqttV5TypesTest, DynamicBinaryDataTest) {
    using namespace Common;

    // Données d'origine
    const char* testStr = "Hello MQTT";
    uint16_t size = static_cast<uint16_t>(std::strlen(testStr));
    DynamicBinaryData original(reinterpret_cast<const uint8_t*>(testStr), size);

    // Vérifie que les données sont valides
    ASSERT_TRUE(original.checkImpl());
    EXPECT_EQ(original.size, size);
    EXPECT_EQ(std::memcmp(original.data, testStr, size), 0);

    // Sérialise les données dans un buffer
    std::vector<uint8_t> buffer(original.getSerializedSize());
    uint32_t serializedSize = original.serialize(buffer.data());

    EXPECT_EQ(serializedSize, size + sizeof(uint16_t));

    // Désérialise dans un nouvel objet
    DynamicBinaryData copy;
    uint32_t deserializedSize = copy.deserialize(buffer.data(), (uint32_t)buffer.size());

    // Vérifie le résultat
    EXPECT_EQ(deserializedSize, serializedSize);
    EXPECT_TRUE(copy.checkImpl());
    EXPECT_EQ(copy.size, size);
    EXPECT_EQ(std::memcmp(copy.data, testStr, size), 0);
}
TEST(MqttV5TypesTest, DynamicBinaryDataViewTest) {
    using namespace Common;

    const uint8_t rawData[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint16_t dataSize = sizeof(rawData);

    // Création de la vue
    DynamicBinaryDataView view(rawData, dataSize);

    // Vérification taille de sérialisation
    EXPECT_EQ(view.getSerializedSize(), dataSize + sizeof(uint16_t));

    // Sérialisation
    std::vector<uint8_t> buffer(view.getSerializedSize());
    uint32_t written = view.serialize(buffer.data());
    EXPECT_EQ(written, buffer.size());

    // Vérifie taille codée en big endian
    EXPECT_EQ(buffer[0], (dataSize >> 8) & 0xFF);
    EXPECT_EQ(buffer[1], dataSize & 0xFF);

    // Vérifie les données copiées
    EXPECT_EQ(std::memcmp(buffer.data() + 2, rawData, dataSize), 0);

    // Désérialisation dans une vue
    DynamicBinaryDataView parsedView;
    uint32_t read = parsedView.deserialize(buffer.data(), (uint32_t)buffer.size());

    EXPECT_EQ(read, buffer.size());
    EXPECT_EQ(parsedView.size, dataSize);
    EXPECT_EQ(std::memcmp(parsedView.data, rawData, dataSize), 0);
}

TEST(MqttV5TypesTest, DynamicStringViewTest) {
    using namespace Common;

    const char* str = "TestView";
    DynamicStringView view(str, 8);
    EXPECT_EQ(view.size, 8);
    EXPECT_EQ(std::memcmp(view.data, str, 8), 0);
}
TEST(MqttV5TypesTests, BigEndianConversion) {
    uint16_t value = 0x1234;
    EXPECT_EQ(BigEndian(value), 0x3412);
}

TEST(MqttV5TypesTests, VBIntAssignment) {
    VBInt vbint;
    vbint = 0x02345678;
    EXPECT_EQ(vbint.size, 4);
}
TEST(MqttV5TypesTests, VBIntCopyConstructor) {
    VBInt vbint1(0x02345678);
    VBInt vbint2(vbint1);
    EXPECT_EQ(vbint2.size, 4);
    EXPECT_EQ(vbint2.word, vbint1.word);
    for (int i = 0; i < vbint1.size; ++i) EXPECT_EQ(vbint2.raw[i], vbint1.raw[i]);
}
TEST(MqttV5TypesTests, VBIntSerialization) {
    VBInt vbint = 0x123456;
    // Vérifiez les valeurs sérialisées
    EXPECT_EQ(vbint.size, 3);
    EXPECT_EQ(vbint.raw[0], 0xD6);  // Octet de poids faible avec bit de continuation
    EXPECT_EQ(vbint.raw[1], 0xE8);  // Octet de poids faible avec bit de continuation
    EXPECT_EQ(vbint.raw[2], 0x48);  // Octet de poids fort sans bit de continuation
    uint8_t buffer[5] = {};
    uint32_t serializedSize = vbint.serialize(buffer);
    EXPECT_EQ(serializedSize, 3);
    EXPECT_EQ(buffer[0], 0xD6);  // Octet de poids faible avec bit de continuation
    EXPECT_EQ(buffer[1], 0xE8);  // Octet de poids faible avec bit de continuation
    EXPECT_EQ(buffer[2], 0x48);  // Octet de poids fort sans bit de continuation
}

TEST(MqttV5TypesTests, MappedVBInt_ReadValidVarInt) {
    // Représente 0x123456 en encodage VarInt MQTT
    uint8_t buffer[] = {0xD6, 0xB4, 0x92, 0x01};

    MappedVBInt view;
    uint32_t result = view.deserialize(buffer, sizeof(buffer));

    EXPECT_EQ(result, 4);
    EXPECT_EQ(view.size, 4);
    EXPECT_TRUE(view.checkImpl());

    // Vérifie la copie via serialize
    uint8_t out[4] = {};
    view.serialize(out);
    EXPECT_EQ(std::memcmp(out, buffer, 4), 0);
}