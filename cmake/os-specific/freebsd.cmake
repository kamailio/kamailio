message(STATUS "Configuring for FreeBSD")

target_compile_definitions(
  common
  INTERFACE HAVE_SOCKADDR_SA_LEN
            HAVE_GETHOSTBYNAME2
            HAVE_UNION_SEMUN
            HAVE_SCHED_YIELD
            HAVE_MSGHDR_MSG_CONTROL
            HAVE_CONNECT_ECONNRESET_BUG
            HAVE_TIMEGM
            HAVE_IP_MREQN)

if(${RAW_SOCKS})
  target_compile_definitions(common INTERFACE USE_RAW_SOCKS)
endif()

if(NOT ${USE_FAST_LOCK})
  target_compile_definitions(common INTERFACE USE_PTHREAD_MUTEX)
endif()

if(NOT ${NO_SELECT})
  target_compile_definitions(common INTERFACE HAVE_SELECT)
endif()

if(NOT ${NO_KQUEUE})
  target_compile_definitions(common INTERFACE HAVE_KQUEUE)
endif()

if(NOT DEFINED RUN_PREFIX)
  set(RUN_PREFIX "/var")
endif()
