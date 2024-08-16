project(fab)
pkg_check_modules (FABRIC libfabric)

if (NOT FABRIC_FOUND)
  buildLibfabirc()
endif()


add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE core ${FABRIC_LIBRARIES})

if (${FABRIC_LIBDIR})
  target_link_directories(${X} INTERFACE ${FABRIC_LIBDIR})
endif()
target_include_directories(${PROJECT_NAME}
  INTERFACE
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${FABRIC_INCLUDEDIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)


