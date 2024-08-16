project(blob)

include(buildSPDK)
build_spdk()

add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE core spdk::spdk)

target_include_directories(${PROJECT_NAME}
  INTERFACE
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${FABRIC_INCLUDEDIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

