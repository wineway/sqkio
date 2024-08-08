project(sched-core)

add_subdirectory(net)
add_subdirectory(core)
if(BUILD_TESTING)
	add_subdirectory(tests)
endif()
