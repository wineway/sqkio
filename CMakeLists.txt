cmake_minimum_required(VERSION 3.10)

project(skq-sched-core VERSION 0.0.1 LANGUAGES CXX)
set(FABRIC_PATH /opt/libfabric)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_TESTING ON)
set(WITH_NET_FABRIC ON)
set(WITH_SPDLOG ON)

include(FetchContent)

if (WITH_SPDLOG)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog
  GIT_TAG v1.x
)
FetchContent_MakeAvailable(spdlog)
add_compile_definitions(HAS_SPDLOG)
endif()

add_subdirectory(src)
