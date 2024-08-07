cmake_minimum_required(VERSION 3.10)

project(skq-sched-core VERSION 0.0.1 LANGUAGES CXX)
set(FABRIC_PATH /opt/libfabric)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_TESTING ON)
set(WITH_NET_FABRIC ON)

add_subdirectory(src)
