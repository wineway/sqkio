project(common)
add_subdirectory(tlsf)

add_library(${PROJECT_NAME} SHARED)

target_include_directories(${PROJECT_NAME}
  PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PUBLIC tlsf)

target_sources(${PROJECT_NAME}
        PRIVATE tlsf_allocator.cc)

if(APPLE)
  execute_process(COMMAND sysctl -n hw.cachelinesize
  OUTPUT_VARIABLE CACHE_LINESIZE
  OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(UNIX)
  execute_process(COMMAND getconf LEVEL1_DCACHE_LINESIZE
  OUTPUT_VARIABLE CACHE_LINESIZE
  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
target_compile_definitions(${PROJECT_NAME} PUBLIC SQK_CACHE_LINESIZE=${CACHE_LINESIZE})

if (WITH_SPDLOG)
target_include_directories(${PROJECT_NAME}
  PUBLIC
		${spdlog_SOURCE_DIR}/include
  )
endif()

if (INSTALL_SQKIO)
  install(TARGETS ${PROJECT_NAME}
EXPORT "${PROJECT_NAME}Targets"
        FILE_SET HEADERS DESTINATION ${SQKIO_INSTALL_INCLUDEDIR}
  )
endif()
