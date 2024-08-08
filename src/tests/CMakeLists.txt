project(sched-core)

add_executable(${PROJECT_NAME}
	core_util.cc
)
target_link_libraries(${PROJECT_NAME} sqk-core)
target_include_directories(${PROJECT_NAME}
	PUBLIC
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(fabric)
