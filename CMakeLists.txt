cmake_minimum_required(VERSION 3.16)
project(ZmqStream LANGUAGES CXX VERSION 1.0.0)

include(cmake/build.cmake)
include(cmake/deps.cmake)

set(HEADERS
    include/RegisterTypes.hpp
    include/stream/ZmqSubscriber.hpp
)

set(SOURCES
    src/RegisterTypes.cpp
    src/stream/ZmqSubscriber.cpp
)

add_library(zmqstream SHARED ${SOURCES} ${HEADERS})

target_include_directories(zmqstream PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zmqstream PRIVATE
    godot::cpp
    PkgConfig::ZMQ
)

get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)

# El nombre debe coincidir con las rutas de demo/bin/zmq_stream.gdextension.
set_target_properties(zmqstream PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "zmqstream${GODOTCPP_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/demo/bin>"
    POSITION_INDEPENDENT_CODE ON
)
