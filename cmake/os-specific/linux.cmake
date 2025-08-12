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
