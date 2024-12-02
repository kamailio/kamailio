cmake_minimum_required(VERSION 3.10)

# set(CMAKE_C_STANDARD 11)
# set(CMAKE_C_STANDARD_REQUIRED True)

# set(locking_methods FAST_LOCK USE_FUTEX USE_PTHREAD_MUTEX USE_POSIX_SEM USE_SYSV_SEM)
# set(locking_method FAST_LOCK CACHE STRING "Locking method to use")

#if defined(FAST_LOCK) || defined(USE_PTHREAD_MUTEX) || defined(USE_POSIX_SEM)
#elif defined USE_SYSV_SEM

# set_property(CACHE locking_method PROPERTY STRINGS ${locking_methods})

# set(kamailio_core_SRC 
#     "lock_ops.c"
#     )

# Set sources found on this folder
file(GLOB kamailio_core_SRC "*.c")

get_target_property(MY_INTERFACE_DEFINITIONS common INTERFACE_COMPILE_DEFINITIONS)
message(STATUS "Compile definitions for common: ${MY_INTERFACE_DEFINITIONS}")

add_library(kamailio_core ${kamailio_core_SRC})
target_include_directories(kamailio_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kamailio_core PRIVATE common)

get_target_property(KAMAILIO_CORE_DEFINITIONS kamailio_core COMPILE_DEFINITIONS)
message(STATUS "Compile definitions for kamailio_core: ${KAMAILIO_CORE_DEFINITIONS}")

# string(TOUPPER ${locking_method} LOCKING_METHOD_UPPER)
# Set some definitions for the target
# target_compile_definitions(kamailio_core PUBLIC USE_FUTEX FAST_LOCK ADAPTIVE_WAIT ADAPTIVE_WAIT_LOOPS=1024)