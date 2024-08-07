project(sqk-net-fab)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
  INTERFACE
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${FABRIC_PATH}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)


