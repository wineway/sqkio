project(blob-test)

add_executable(blob-test
	blob_test.cc
)

foreach(X IN ITEMS blob-test)
  target_include_directories(${X} PRIVATE ${SPDLOG_SOURCE_DIR}/include)
  target_link_libraries(${X} blob spdlog)
  if (INSTALL_SQKIO)
    install(TARGETS ${X}
    RUNTIME DESTINATION ${SQKIO_INSTALL_BINDIR})
  endif()
endforeach()

