message(
  STATUS
    "Configuring for Darwin (Apple stationary operating systems (macOS, OS X, etc.)"
)

target_compile_definitions(
  common
  INTERFACE HAVE_SOCKADDR_SA_LEN
            HAVE_GETHOSTBYNAME2
            HAVE_UNION_SEMUN
            HAVE_SCHED_YIELD
            USE_ANON_MMAP
            HAVE_MSGHDR_MSG_CONTROL
            NDEBUG # NDEBUG used to turn off assert (assert wants to call
            # eprintf which doesn't seem to be defined in any shared lib
            HAVE_CONNECT_ECONNRESET_BUG
            HAVE_TIMEGM
            USE_SIGWAIT
            HAVE_IP_MREQN
)

target_link_libraries(common INTERFACE resolv)

message(STATUS "USE_FAST_LOCK = ${USE_FAST_LOCK}")
if(NOT ${USE_FAST_LOCK})
  target_compile_definitions(common INTERFACE USE_PTHREAD_MUTEX USE_SYSV_SEM)
endif()

set(CMAKE_MODULE_LINKER_FLAGS
    "${CMAKE_MODULE_LINKER_FLAGS} -bundle -flat_namespace -undefined suppress"
)
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -dynamiclib -flat_namespace -undefined suppress"
)

if(NOT DEFINED ${NO_SELECT})
  target_compile_definitions(common INTERFACE HAVE_SELECT)
endif()

if(NOT DEFINED ${NO_KQUEUE})
  target_compile_definitions(common INTERFACE HAVE_KQUEUE)
endif()

if(NOT DEFINED RUN_PREFIX)
  set(RUN_PREFIX "/var")
endif()
