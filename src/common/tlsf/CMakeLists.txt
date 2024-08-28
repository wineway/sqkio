project(tlsf VERSION 2.4.6 LANGUAGES C)

add_library(${PROJECT_NAME} SHARED)

target_include_directories(${PROJECT_NAME}
  PUBLIC
		$<INSTALL_INTERFACE:include>
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} PRIVATE
        tlsf.c)
