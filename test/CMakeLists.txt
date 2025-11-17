# CMakeList.txt for Mqtt unit tests
#
# © 2025 by Hatem Nabli

cmake_minimum_required(VERSION 3.8)
set(this MqttV5Tests)

set(SOURCES
    src/MqttV5PacketsTests.cpp
    src/MqttV5PropertiesTests.cpp
    src/MqttV5TypesTests.cpp
    src/MqttCircularBufferTests.cpp
    src/MqttV5ClientTests.cpp
)

add_executable(${this} ${SOURCES} )
set_target_properties(${this} PROPERTIES
    FOLDER Tests
)

target_include_directories(${this} PRIVATE ..)

target_link_libraries(${this}
    gtest_main
    StringUtils
    MqttV5
    Utf8
    Uri
)

add_test(
    NAME ${this}
    COMMAND ${this}
)