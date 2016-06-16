PROJECT(ssl-radio-protocol)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)

INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR})

FILE(GLOB ProtoFiles "proto/*.proto")
PROTOBUF_GENERATE_CPP(ProtoSource ProtoHeaders ${ProtoFiles})

ADD_LIBRARY(protobuf_all ${ProtoSource})
TARGET_LINK_LIBRARIES(protobuf_all ${PROTOBUF_LIBRARIES})

SET(target server)
ADD_EXECUTABLE(${target} src/server_main.cpp)
TARGET_LINK_LIBRARIES(${target} protobuf_all)

SET(target client)
ADD_EXECUTABLE(${target} src/client_main.cpp)
TARGET_LINK_LIBRARIES(${target} protobuf_all)
