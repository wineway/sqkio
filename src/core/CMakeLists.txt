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

if (WITH_SPDLOG)
target_include_directories(${PROJECT_NAME}
  INTERFACE
		${spdlog_SOURCE_DIR}/include
  )
endif()

if(APPLE)
  execute_process(COMMAND sysctl -n hw.cachelinesize
  OUTPUT_VARIABLE CACHE_LINESIZE
  OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(UNIX)
  execute_process(COMMAND getconf LEVEL1_DCACHE_LINESIZE
  OUTPUT_VARIABLE CACHE_LINESIZE
  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
target_compile_definitions(${PROJECT_NAME} INTERFACE SQK_CACHE_LINESIZE=${CACHE_LINESIZE})

if (INSTALL_SQKIO)
set(test_dir /tmp)
  install(TARGETS ${PROJECT_NAME}
EXPORT "${PROJECT_NAME}Targets"
        FILE_SET HEADERS DESTINATION ${SQKIO_INSTALL_INCLUDEDIR}
  )
endif()

