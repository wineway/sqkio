project(core)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
  INTERFACE
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME}
        INTERFACE FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES core.hpp log.hpp)

target_link_libraries(${PROJECT_NAME} INTERFACE common)
if (INSTALL_SQKIO)
set(test_dir /tmp)
  install(TARGETS ${PROJECT_NAME}
EXPORT "${PROJECT_NAME}Targets"
        FILE_SET HEADERS DESTINATION ${SQKIO_INSTALL_INCLUDEDIR}
  )
endif()

