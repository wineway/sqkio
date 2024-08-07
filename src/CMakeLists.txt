project(sched-core)

# add_library(${PROJECT_NAME} SHARED
# 	sched.cc
# )

# add_library(libs::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

add_subdirectory(net)
if(BUILD_TESTING)
	add_subdirectory(tests)
endif()
