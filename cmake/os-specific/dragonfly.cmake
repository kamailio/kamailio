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

if(NOT ${USE_FAST_LOCK})
  target_compile_definitions(common INTERFACE USE_PTHREAD_MUTEX)
  target_link_libraries(common INTERFACE pthread)
else()
  # Check if lock_method is posix or pthread
  if(LOCK_METHOD STREQUAL "USE_POSIX_SEM" OR LOCK_METHOD STREQUAL "USE_PTHREAD_MUTEX")
    message(STATUS "Using ${LOCK_METHOD} for locks")
    target_link_libraries(common INTERFACE pthread)
  endif()

endif()

if(NOT NO_SELECT)
  target_compile_definitions(common INTERFACE HAVE_SELECT)
endif()

if(NOT NO_EPOLL)
  target_compile_definitions(common INTERFACE HAVE_EPOLL)
endif()

if(NOT NO_SIGIO_RT)
  target_compile_definitions(common INTERFACE HAVE_SIGIO_RT SIGINFO64_WORKAROUND)
endif()
