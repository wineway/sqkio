project(common-test)

add_executable(ring-test
	ring_test.cc
)
foreach(X IN ITEMS ring-test)
  target_include_directories(${X} PRIVATE ${SPDLOG_SOURCE_DIR}/include)
  target_link_libraries(${X} common)
  if (INSTALL_SQKIO)
    install(TARGETS ${X}
    RUNTIME DESTINATION ${SQKIO_INSTALL_BINDIR})
  endif()
endforeach()

add_test(NAME RING_TEST COMMAND ring-test)
