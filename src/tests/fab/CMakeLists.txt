project(sched-fab)

add_executable(raw_bin
	fab.cc
)
add_executable(wrapped_bin
	fab2.cc
)

foreach(X IN ITEMS raw_bin wrapped_bin)
  target_link_libraries(${X} sqk-net-fab fabric)
  target_link_directories(${X} PRIVATE ${FABRIC_PATH}/lib)
endforeach()


