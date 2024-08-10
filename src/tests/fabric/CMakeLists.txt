project(fab-test)

add_executable(raw-bin
	raw_fab_util.c
)
add_executable(wrapped-bin
	fabric_util.cc
)

foreach(X IN ITEMS raw-bin wrapped-bin)
  target_include_directories(${X} PRIVATE ${SPDLOG_SOURCE_DIR}/include)
  target_link_libraries(${X} fab ${FABRIC_LIBRARIES} spdlog)
  target_link_directories(${X} PUBLIC ${FABRIC_LIBDIR})
  if (INSTALL_SQKIO)
    install(TARGETS ${X}
    RUNTIME DESTINATION ${SQKIO_INSTALL_BINDIR})
  endif()
endforeach()



