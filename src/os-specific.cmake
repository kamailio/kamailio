# Quite analogous to the Makefile.defs file
# This file is used to define the common flags and options for the project
# The flags are defined as INTERFACE properties of the common library
# The flags are then used by the other libraries and executables
cmake_minimum_required(VERSION 3.10)

# Linux specific flags
if(UNIX AND NOT APPLE)
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
endif()
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra -Wpedantic -Werror")
    # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
    # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")