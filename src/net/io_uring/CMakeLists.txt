project(uring)
pkg_check_modules (URING liburing)

add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE core ${URING_LIBRARIES})

target_include_directories(${PROJECT_NAME}
  INTERFACE
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${URING_INCLUDEDIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
