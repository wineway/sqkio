project(sched-core)

# add_library(${PROJECT_NAME} SHARED
# 	sched.cc
# )

# add_library(libs::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

add_executable(${PROJECT_NAME}
	sched.cc
)
target_include_directories(${PROJECT_NAME}
	PUBLIC
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(fab)
