project(blob)

include(buildSPDK)
build_spdk()

add_executable(blob "")
target_include_directories(blob PUBLIC ${SPDK_INCLUDE_DIR} ${spdlog_SOURCE_DIR}/include)
target_link_libraries(blob spdk::spdk spdlog core)

target_sources(blob
  PRIVATE
  "blob.cc"
)


target_include_directories(blob
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


target_compile_definitions(blob
  PRIVATE
)


