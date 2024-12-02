# Quite analogous to the Makefile.defs file
# This file is used to define the common flags and options for the project
# The flags are defined as INTERFACE properties of the common library
# The flags are then used by the other libraries and executables
cmake_minimum_required(VERSION 3.10)

# Linux specific flags
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(STATUS "Configuring for Linux")
  target_compile_definitions(common INTERFACE
                HAVE_GETHOSTBYNAME2
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
    target_compile_definitions(common INTERFACE
                USE_RAW_SOCKS
                )
  endif()
  
  message(STATUS "USE_FAST_LOCK = ${USE_FAST_LOCK}")
  if(NOT ${USE_FAST_LOCK})
    target_compile_definitions(common INTERFACE
                USE_PTHREAD_MUTEX
                )
    target_link_libraries(common INTERFACE pthread)

  else()
    # Check if lock_method is posix or pthread
    if(LOCK_METHOD STREQUAL "USE_POSIX_SEM" OR LOCK_METHOD STREQUAL "USE_PTHREAD_MUTEX" )
      message(STATUS "Using ${LOCK_METHOD} for locks")
      target_link_libraries(common INTERFACE pthread)
    endif()

  endif()

  if(NOT DEFINED ${NO_SELECT})
    target_compile_definitions(common INTERFACE
                HAVE_SELECT
                )
  endif()

  if(NOT DEFINED ${NO_EPOLL})
    target_compile_definitions(common INTERFACE
                HAVE_EPOLL
                )
  endif()

  if(NOT DEFINED ${NO_SIGIO})
    target_compile_definitions(common INTERFACE
                HAVE_SIGIO_RT 
                SIGINFO64_WORKAROUND
                )
  endif()

endif()

# DragonFly BSD specific flags
if(CMAKE_SYSTEM_NAME STREQUAL "DragonFly")
  message(STATUS "Configuring for DragonFly BSD")
  target_compile_definitions(common INTERFACE
                HAVE_SOCKADDR_SA_LEN
                HAVE_GETHOSTBYNAME2
                HAVE_UNION_SEMUN
                HAVE_SCHED_YIELD
                HAVE_MSGHDR_MSG_CONTROL
                HAVE_CONNECT_ECONNRESET_BUG
                HAVE_TIMEGM
                HAVE_NETINET_IN_SYSTM
                )
  message(STATUS "USE_FAST_LOCK = ${USE_FAST_LOCK}")
  if(NOT ${USE_FAST_LOCK})
    target_compile_definitions(common INTERFACE
                USE_PTHREAD_MUTEX
                )
    target_link_libraries(common INTERFACE pthread)
  endif()

  if(NOT DEFINED ${NO_KQUEUE})
    target_compile_definitions(common INTERFACE
                HAVE_KQUEUE
                )
  endif()

  if(NOT DEFINED ${NO_SELECT})
    target_compile_definitions(common INTERFACE
                HAVE_SELECT
                )
  endif()
  
endif()
