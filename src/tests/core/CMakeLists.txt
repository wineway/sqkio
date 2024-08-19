project(core-test)

add_executable(core-bench
	core_bench.cc
)

foreach(X IN ITEMS core-bench)
  target_include_directories(${X} PRIVATE ${SPDLOG_SOURCE_DIR}/include)
  target_link_libraries(${X} PRIVATE core nanobench)
  if (INSTALL_SQKIO)
    install(TARGETS ${X}
    RUNTIME DESTINATION ${SQKIO_INSTALL_BINDIR})
  endif()
endforeach()

