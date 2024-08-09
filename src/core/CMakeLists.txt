project(sqk-core)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
  INTERFACE
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (WITH_SPDLOG)
target_include_directories(${PROJECT_NAME}
  INTERFACE
		${spdlog_SOURCE_DIR}/include
  )
endif()


