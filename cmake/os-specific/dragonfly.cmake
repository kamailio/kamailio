# Linux specific flags
message(STATUS "Configuring for Linux")
target_compile_definitions(
  common
  INTERFACE HAVE_GETHOSTBYNAME2
            HAVE_UNION_SEMUN
            HAVE_SCHED_YIELD
            HAVE_MSG_NOSIGNAL
            HAVE_MSGHDR_MSG_CONTROL
            HAVE_ALLOCA_H
            HAVE_TIMEGM
            HAVE_SCHED_SETSCHEDULER
            HAVE_IP_MREQN
)
if(${RAW_SOCKS})
  target_compile_definitions(common INTERFACE USE_RAW_SOCKS)
endif()

message(STATUS "USE_FAST_LOCK = ${USE_FAST_LOCK}")
if(NOT ${USE_FAST_LOCK})
  target_compile_definitions(common INTERFACE USE_PTHREAD_MUTEX)
  target_link_libraries(common INTERFACE pthread)
else()
  # Check if lock_method is posix or pthread
  if(LOCK_METHOD STREQUAL "USE_POSIX_SEM" OR LOCK_METHOD STREQUAL
                                             "USE_PTHREAD_MUTEX"
  )
    message(STATUS "Using ${LOCK_METHOD} for locks")
    target_link_libraries(common INTERFACE pthread)
  endif()

endif()

if(NOT DEFINED ${NO_SELECT})
  target_compile_definitions(common INTERFACE HAVE_SELECT)
endif()

if(NOT DEFINED ${NO_EPOLL})
  target_compile_definitions(common INTERFACE HAVE_EPOLL)
endif()

if(NOT DEFINED ${NO_SIGIO})
  target_compile_definitions(
    common INTERFACE HAVE_SIGIO_RT SIGINFO64_WORKAROUND
  )
endif()
