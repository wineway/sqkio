cmake_minimum_required(VERSION 3.23.1)

project(sqkio VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_TESTING ON)
set(WITH_NET_FABRIC OFF)
set(WITH_SPDLOG ON)
set(INSTALL_SQKIO ON)
set(WITH_IO_SPDK ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(FetchContent)
include(FindPkgConfig)
include(GNUInstallDirs)
include(buildLibfabric)
include(CTest)

if (WITH_SPDLOG)
  FetchContent_Declare(
    SPDLOG
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG v1.x
  )
  FetchContent_MakeAvailable(SPDLOG)
  add_compile_definitions(HAS_SPDLOG)
endif()

if(APPLE)
  set(WITH_IO_SPDK OFF)
endif()
add_subdirectory(src)

if (INSTALL_SQKIO)
  set(SQKIO_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/sqkio")
  set(SQKIO_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/sqkio")
  set(SQKIO_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
  set(SQKIO_INSTALL_PKGCFG_DIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
  set(pkg_config "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc")

  if(IS_ABSOLUTE "${SQKIO_INSTALL_INCLUDEDIR}")
    set(PKG_CONFIG_INCLUDEDIR "${SQKIO_INSTALL_INCLUDEDIR}")
  else()
    set(PKG_CONFIG_INCLUDEDIR "\${prefix}/${SQKIO_INSTALL_INCLUDEDIR}")
  endif()
  if(IS_ABSOLUTE "${SQKIO_INSTALL_LIBDIR}")
    set(PKG_CONFIG_LIBDIR "${SQKIO_INSTALL_LIBDIR}")
  else()
    set(PKG_CONFIG_LIBDIR "\${exec_prefix}/${SQKIO_INSTALL_LIBDIR}")
  endif()
  get_target_property(PKG_CONFIG_DEFINES core INTERFACE_COMPILE_DEFINITIONS)
  string(REPLACE ";" " -D" PKG_CONFIG_DEFINES "${PKG_CONFIG_DEFINES}")
  string(CONCAT PKG_CONFIG_DEFINES "-D" "${PKG_CONFIG_DEFINES}")
  set(PKG_CONFIG_REQUIRES fabric)
  configure_file("cmake/${PROJECT_NAME}.pc.in" "${pkg_config}" @ONLY)
  install(FILES "${pkg_config}" DESTINATION "${SQKIO_INSTALL_PKGCFG_DIR}")
endif()

