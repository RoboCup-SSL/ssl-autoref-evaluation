PROJECT(ssl-radio-protocol)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)

INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR})

FILE(GLOB ProtoFiles "proto/*.proto")
PROTOBUF_GENERATE_CPP(ProtoSource ProtoHeaders ${ProtoFiles})

SET(libs pthread)

INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src)

ADD_LIBRARY(protobuf_all ${ProtoSource})
TARGET_LINK_LIBRARIES(protobuf_all ${PROTOBUF_LIBRARIES})

ADD_LIBRARY(shared_lib
            src/shared/netraw.cpp
            src/shared/pthread_utils.cpp)

SET(target logger)
ADD_EXECUTABLE(${target} src/logger_main.cpp)
TARGET_LINK_LIBRARIES(${target} protobuf_all shared_lib ${libs})

SET(target playback)
ADD_EXECUTABLE(${target} src/playback_main.cpp)
TARGET_LINK_LIBRARIES(${target} protobuf_all shared_lib ${libs})


SET(target evaluate)
ADD_EXECUTABLE(${target} src/evaluate_main.cpp)
TARGET_LINK_LIBRARIES(${target} protobuf_all shared_lib ${libs})
