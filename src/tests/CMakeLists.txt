project(core-test)

add_executable(${PROJECT_NAME}
	core_util.cc
)
add_test(NAME SCHED_TEST COMMAND ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} core)
target_include_directories(${PROJECT_NAME}
	PUBLIC
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

if (INSTALL_SQKIO)
  install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION ${SQKIO_INSTALL_BINDIR})
endif()
add_subdirectory(fabric)
add_subdirectory(io/blob)
