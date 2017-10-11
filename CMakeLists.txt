#cmake_minimum_required(VERSION 3.8)
project(SSFS)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DMUDUO_STD_STRING)

#include_directories(../include)
#link_directories(../lib)

set(SRCDIR src)
include_directories(include)

#masterserver
set(MSEXE masterserver)
set(MS_SRC_DIR ${SRCDIR}/MasterServer)
file(GLOB MS_SF ${MS_SRC_DIR}/*.cpp ${MS_SRC_DIR}/*.h)
add_executable(${MSEXE} ${MS_SF})
target_link_libraries(${MSEXE} muduo_net_cpp11 muduo_base_cpp11 pthread jsoncpp)


#chunkserver
set(CSEXE chunkserver)
set(CS_SRC_DIR ${SRCDIR}/ChunkServer)
file(GLOB CS_SF ${CS_SRC_DIR}/*.cpp ${CS_SRC_DIR}/*.h)
add_executable(${CSEXE} ${CS_SF})
target_link_libraries(${CSEXE} muduo_net_cpp11 muduo_base_cpp11 pthread jsoncpp)


