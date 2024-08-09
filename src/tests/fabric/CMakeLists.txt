project(sched-fab)

add_executable(raw_bin
	raw_fab_util.c
)
add_executable(wrapped_bin
	fabric_util.cc
)

foreach(X IN ITEMS raw_bin wrapped_bin)
  target_include_directories(${X} PRIVATE ${SPDLOG_SOURCE_DIR}/include)
  target_link_libraries(${X} sqk-net-fab ${FABRIC_LIBRARIES} spdlog)
endforeach()


