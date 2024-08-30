project(core-test)

include(FetchContent)

FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG v4.3.11
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nanobench)

add_compile_options("-g")

add_executable(${PROJECT_NAME}
	core_util.cc
)
add_test(NAME SCHED_TEST COMMAND ${PROJECT_NAME} "simple")
add_test(NAME CORO_EXCEPTION_TEST COMMAND ${PROJECT_NAME} "exception_propagation")
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
add_subdirectory(common)
add_subdirectory(core)
if (WITH_NET_FABRIC)
add_subdirectory(fabric)
endif()
if (WITH_IO_SPDK)
add_subdirectory(io/blob)
endif()
