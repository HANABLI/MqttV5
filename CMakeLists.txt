# CMakeLists.txt for Json Module
#
# © 2025 by Hatem Nabli

cmake_minimum_required(VERSION 3.8)
set(this MqttV5)

set(Headers
    include/MqttV5/MqttV5Properties.hpp
    include/MqttV5/MqttV5Packets/MqttV5Packets.hpp
    include/MqttV5/MqttV5Types.hpp
    include/MqttV5/MqttV5Constants.hpp
    include/MqttV5/MqttV5ISerializable.hpp
    include/MqttV5/MqttV5Core/ClientTransportLayer.hpp
    include/MqttV5/MqttV5Core/Connection.hpp
    include/MqttV5/MqttV5Core/MqttClient.hpp
    include/MqttV5/MqttV5Core/TimeKeeper.hpp
)

set(Sources
    src/MqttV5Properties.cpp
    src/MqttV5Packets.cpp
    src/MqttClient.cpp
)

add_library(${this} STATIC ${Sources} ${Headers})
set_target_properties(${this} PROPERTIES 
    FOLDER Libraries
)

target_include_directories(${this} PUBLIC include)
target_link_libraries(${this} PUBLIC
    StringUtils
    SystemUtils
    Utf8
)

add_subdirectory(test)
