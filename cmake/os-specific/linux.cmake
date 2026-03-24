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

if(${RAW_SOCKS})
  target_compile_definitions(common INTERFACE USE_RAW_SOCKS)
endif()
