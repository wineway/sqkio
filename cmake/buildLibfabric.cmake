include(FetchContent)

find_program(MAKE_EXE NAMES gmake nmake make)
include(ExternalProject)
FetchContent_Declare(FABRIC
GIT_REPOSITORY https://github.com/ofiwg/libfabric.git
GIT_TAG main)
FetchContent_MakeAvailable(FABRIC)
macro(buildLibfabirc)
    add_library(fabric SHARED IMPORTED)
    set_target_properties(fabric PROPERTIES
      IMPORTED_LOCATION "${fabric_SOURCE_DIR}/src/.libs/${CMAKE_SHARED_LIBRARY_PREFIX}fabric${CMAKE_SHARED_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${fabric_SOURCE_DIR}/include"
    )
    ExternalProject_Add(fabric-ext
    SOURCE_DIR ${fabric_SOURCE_DIR}
    CONFIGURE_COMMAND ./autogen.sh COMMAND ./configure
    BUILD_COMMAND ${make_cmd}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_MERGED_STDOUTERR ON
    LOG_OUTPUT_ON_FAILURE ON)
    add_dependencies(fabric fabric-ext)
    set(FABRIC_LIBRARIES fabric)
    set(BUILD_FABRIC ON)
endmacro()

