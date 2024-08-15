include(FetchContent)
include(FindMake)
find_make("MAKE_EXECUTABLE" "make_cmd")
include(ExternalProject)
FetchContent_Declare(FABRIC
GIT_REPOSITORY https://github.com/ofiwg/libfabric.git
GIT_TAG main)

macro(buildLibfabirc)
    FetchContent_MakeAvailable(FABRIC)
    if (CMAKE_BUILD_TYPE MATCHES Debug)
      set(ENABLE_DEBUG "--enable-debug")
      else()
      set(ENABLE_DEBUG "")
      message("build libfabric without disable debug")
    endif()
    add_library(fabric SHARED IMPORTED)
    set_target_properties(fabric PROPERTIES
      IMPORTED_LOCATION "${fabric_SOURCE_DIR}/src/.libs/${CMAKE_SHARED_LIBRARY_PREFIX}fabric${CMAKE_SHARED_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${fabric_SOURCE_DIR}/include"
    )
    ExternalProject_Add(fabric-ext
    SOURCE_DIR ${fabric_SOURCE_DIR}
    CONFIGURE_COMMAND ./autogen.sh COMMAND ./configure ${ENABLE_DEBUG}
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

