FetchContent_Declare(SPDK_SRC
GIT_REPOSITORY https://github.com/spdk/spdk.git
GIT_TAG master)

macro(build_spdk)
  FetchContent_MakeAvailable(SPDK_SRC)
  find_package(aio REQUIRED)
  find_package(uuid REQUIRED)
  find_package(OpenSSL REQUIRED)
  find_package(NUMA REQUIRED)
  include(FindMake)
  find_make("MAKE_EXECUTABLE" "make_cmd")

  set(spdk_CFLAGS "-fPIC")
  include(CheckCCompilerFlag)
  check_c_compiler_flag("-Wno-address-of-packed-member" HAVE_WARNING_ADDRESS_OF_PACKED_MEMBER)
  if(HAVE_WARNING_ADDRESS_OF_PACKED_MEMBER)
    string(APPEND spdk_CFLAGS " -Wno-address-of-packed-member")
  endif()
  check_c_compiler_flag("-Wno-unused-but-set-variable"
    HAVE_UNUSED_BUT_SET_VARIABLE)
  if(HAVE_UNUSED_BUT_SET_VARIABLE)
    string(APPEND spdk_CFLAGS " -Wno-unused-but-set-variable")
  endif()

  include(ExternalProject)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|x86_64|AMD64")
    # a safer option than relying on the build host's arch
    set(target_arch core2)
  else()
    # default arch used by SPDK
    set(target_arch native)
  endif()

  set(source_dir "${spdk_src_SOURCE_DIR}")
message("source: ${spdk_src_SOURCE_DIR}")
  foreach(c accel
            accel_error
            accel_ioat
            bdev
            bdev_aio
            bdev_delay
            bdev_error
            bdev_ftl
            bdev_gpt
            bdev_lvol
            bdev_malloc
            bdev_null
            bdev_nvme
            bdev_passthru
            bdev_raid
            bdev_split
            bdev_zone_block
            blob
            blob_bdev
            blobfs
            blobfs_bdev
            conf
            dma
            env_dpdk
            env_dpdk_rpc
            event
            event_accel
            event_bdev
            event_iobuf
            event_keyring
            event_nbd
            event_scheduler
            event_scsi
            event_sock
            event_vmd
            ftl
            init
            ioat
            json
            jsonrpc
            keyring
            keyring_file
            log
            lvol
            nbd
            notify
            nvme
            rpc
            scheduler_dpdk_governor
            scheduler_dynamic
            scheduler_gscheduler
            scsi
            sock
            sock_posix
            thread
            trace
            trace_parser
            util
            vfio_user
            vmd)
    add_library(spdk::${c} STATIC IMPORTED)
    set(lib_path "${source_dir}/build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}spdk_${c}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set_target_properties(spdk::${c} PROPERTIES
      IMPORTED_LOCATION "${lib_path}"
      INTERFACE_INCLUDE_DIRECTORIES "${source_dir}/include")
    list(APPEND spdk_libs "${lib_path}")
    list(APPEND SPDK_LIBRARIES spdk::${c})
  endforeach()

  ExternalProject_Add(spdk-ext
    SOURCE_DIR ${source_dir}
    CONFIGURE_COMMAND ./configure
      --without-crypto
      --without-golang
      --without-vhost
      --without-fuse
      --without-nvme-cuse
      --without-daos
      --without-rbd
      --without-fio
      --without-dpdk
      --without-virtio
      --without-vhost
      --without-xnvme
      --without-vbdev-compress
      --disable-unit-tests
      --disable-tests
      --enable-debug
      --target-arch=${target_arch}
    # unset $CFLAGS, otherwise it will interfere with how SPDK sets
    # its include directory.
    # unset $LDFLAGS, otherwise SPDK will fail to mock some functions.
    BUILD_COMMAND ${make_cmd} EXTRA_CFLAGS=${spdk_CFLAGS}
    BUILD_IN_SOURCE 1
    BUILD_BYPRODUCTS ${spdk_libs}
    INSTALL_COMMAND ""
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_MERGED_STDOUTERR ON
    LOG_OUTPUT_ON_FAILURE ON)
  unset(make_cmd)
  foreach(spdk_lib ${SPDK_LIBRARIES})
    add_dependencies(${spdk_lib} spdk-ext)
  endforeach()

  set(dpdk_dir ${source_dir}/dpdk)
  set(DPDK_INCLUDE_DIR ${dpdk_dir}/include)
  # create the directory so cmake won't complain when looking at the imported
  # target
  file(MAKE_DIRECTORY ${DPDK_INCLUDE_DIR})
  list(APPEND dpdk_components
    bus_pci
    bus_vdev
    cmdline
    compressdev
    cryptodev
    dmadev
    eal
    ethdev
    hash
    kvargs
    log
    mbuf
    mempool
    mempool_ring
    meter
    net
    pci
    power
    rcu
    reorder
    ring
    security
    telemetry
    timer
    vhost)
  foreach(c ${dpdk_components})
    add_library(dpdk::${c} STATIC IMPORTED)
    add_dependencies(dpdk::${c} dpdk-ext)
    set(dpdk_${c}_LIBRARY
      "${dpdk_dir}/build/lib/${CMAKE_STATIC_LIBRARY_PREFIX}rte_${c}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set_target_properties(dpdk::${c} PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES ${DPDK_INCLUDE_DIR}
      INTERFACE_LINK_LIBRARIES dpdk::cflags
      IMPORTED_LOCATION "${dpdk_${c}_LIBRARY}")
    list(APPEND DPDK_LIBRARIES dpdk::${c})
    list(APPEND DPDK_ARCHIVES "${dpdk_${c}_LIBRARY}")
  endforeach()

  add_library(dpdk::dpdk INTERFACE IMPORTED)
  add_dependencies(dpdk::dpdk
    ${DPDK_LIBRARIES})
  # workaround for https://gitlab.kitware.com/cmake/cmake/issues/16947
  set_target_properties(dpdk::dpdk PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${DPDK_INCLUDE_DIR}
    INTERFACE_LINK_LIBRARIES
    "-Wl,--whole-archive $<JOIN:${DPDK_ARCHIVES}, > -Wl,--no-whole-archive ${dpdk_numa} -Wl,-lpthread,-ldl")
  if(dpdk_rte_CFLAGS)
    set_target_properties(dpdk::dpdk PROPERTIES
      INTERFACE_COMPILE_OPTIONS "${dpdk_rte_CFLAGS}")
  endif()


  set(SPDK_INCLUDE_DIR "${source_dir}/include")
  add_library(spdk::spdk INTERFACE IMPORTED)
  add_dependencies(spdk::spdk
    ${SPDK_LIBRARIES})
  # workaround for https://review.spdk.io/gerrit/c/spdk/spdk/+/6798
  set_target_properties(spdk::spdk PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${SPDK_INCLUDE_DIR}
    INTERFACE_LINK_LIBRARIES
    "-Wl,--whole-archive $<JOIN:${spdk_libs}, > -Wl,--no-whole-archive;dpdk::dpdk;rt;${UUID_LIBRARIES};${AIO_LIBRARIES};${OPENSSL_LIBRARIES};${NUMA_LIBRARIES}")
  unset(source_dir)
endmacro()

