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

target_link_libraries(common INTERFACE ${CMAKE_DL_LIBS} resolv)
target_link_libraries(common_utils INTERFACE resolv)

if(${RAW_SOCKS})
  target_compile_definitions(common INTERFACE USE_RAW_SOCKS)
endif()

if(NOT ${USE_FAST_LOCK})
  target_compile_definitions(common INTERFACE USE_PTHREAD_MUTEX)
  target_link_libraries(common INTERFACE pthread)
  message(STATUS "FAST_LOCK not available on this platform, using: USE_PTHREAD_MUTEX")
else()
  # TODO: Check if this can be reached. Right now it is not possible to set
  # LOCK_METHOD, only USE_FAST_LOCK. This branch is reached when USE_FAST_LOCK
  # is set to true (meaning it is available on platform).
  # Check if lock_method is posix or pthread
  # if(LOCK_METHOD STREQUAL "USE_POSIX_SEM" OR LOCK_METHOD STREQUAL
  #                                            "USE_PTHREAD_MUTEX")
  #   message(STATUS "Using ${LOCK_METHOD} for locks")
  #   target_link_libraries(common INTERFACE pthread)
  # endif()
endif()

if(NOT NO_SELECT)
  target_compile_definitions(common INTERFACE HAVE_SELECT)
endif()

# TODO introduce check for epoll
if(NOT NO_EPOLL)
  target_compile_definitions(common INTERFACE HAVE_EPOLL)
endif()

# TODO introduce check for sigio
if(NOT NO_SIGIO_RT)
  target_compile_definitions(common INTERFACE HAVE_SIGIO_RT SIGINFO64_WORKAROUND)
endif()

# TODO This is placed here to match the Makefiles where both
# FAST_LOCK and USE_FUTEX are defined unconditionally.
# This also had the side effect that futex also uses the definitions
# defined for FAST_LOCK like ADAPTIVE_WAIT[_LOOPS].
# https://github.com/kamailio/kamailio/pull/4363 aims to fix this
# and we should decide whether to also apply the definitions for USE_FUTEX case.
find_path(FUTEX_HEADER_DIR linux/futex.h)
if(FUTEX_HEADER_DIR)
  target_compile_definitions(common INTERFACE USE_FUTEX)
endif()
