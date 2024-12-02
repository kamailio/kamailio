# Quite analogous to the Makefile.defs file
# This file is used to define the common flags and options for the project
# The flags are defined as INTERFACE properties of the common library
# The flags are then used by the other libraries and executables
cmake_minimum_required(VERSION 3.10)

add_library(common INTERFACE)

message(STATUS "CMAKE_C_COMPILER_VERSION: ${CMAKE_C_COMPILER_VERSION}")

set(OS ${CMAKE_SYSTEM_NAME})
message(STATUS "OS: ${OS}")

set(OSREL ${CMAKE_SYSTEM_VERSION})
message(STATUS "OS version: ${OSREL}")
# Find the architecture and from in the format __CPU_arch
set(HOST_ARCH "__CPU_${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "Processor: ${HOST_ARCH}")

# Flavor of the project
# flavour: sip-router, ser or kamailio
# This is used to define the MAIN_NAME flag
# TODO: Kamailio only
set(flavours kamailio)
set(FLAVOUR "kamailio" CACHE STRING "Flavour of the project")
set_property(CACHE FLAVOUR PROPERTY STRINGS ${flavours})


# Verbose option (for debugging purposes) (was quiet in Makefile.defs)
# Probably not needed in CMake and can be removed
# Use the -DCMAKE_VERBOSE_MAKEFILE=ON option to enable verbose mode
option(VERBOSE "Verbose " OFF)
if(VERBOSE)
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif()



option(KMSTATS "Kamailio statistics" ON)
option(FMSTATS "Fast memory statistics" ON)
option(WITHAS "With Application server support" ON)
option(SCTP "SCTP support" ON)
option(RAW_SOCKS "Raw sockets support" ON)
option(MEMPKG "Package memory or sys " ON)


set(memory_manager_switcher f_malloc q_malloc tlsf_malloc)
set(MEMMNG f_malloc CACHE STRING "Memory manager")
set_property(CACHE MEMMNG PROPERTY STRINGS ${memory_manager_switcher})

# if(${MEMPKG})
#   target_compile_definitions(common INTERFACE PKG_MALLOC)
# else()
#   if(${MEMDBGSYS})
#     target_compile_definitions(common INTERFACE DDBG_SYS_MEMORY)
# endif()
# endif()


# -----------------------
#  TLS support
# -----------------------
# TLS support
set(CORE_TLS "" CACHE STRING "CORE_TLS")
set(TLS_HOOKS ON CACHE BOOL "TLS hooks support")

if(${CORE_TLS})
  set(RELEASE "${RELEASE}-tls")
  set(TLS_HOOKS 0)
endif()

if(${TLS_HOOKS} )
  # set(RELEASE "${RELEASE}-tls")
endif()

set(LIBSSL_SET_MUTEX_SHARED ON CACHE BOOL "enable workaround for libssl 1.1+ to set shared mutex attribute")
if(NOT ${LIBSSL_SET_MUTEX_SHARED})
    message(STATUS "Checking if can enable workaround for libssl 1.1+ to set shared mutex attribute")
    message(STATUS "Cross compile: ${CROSS_COMPILE}")
    if(NOT DEFINED CROSS_COMPILE OR NOT ${CROSS_COMPILE} )
        message(STATUS "Checking for OpenSSL 1.1.0")
        find_package(OpenSSL 1.1.0)
        if(OPENSSL_FOUND)
            message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
            if(${OPENSSL_VERSION} VERSION_GREATER_EQUAL  "1.1.0")
                message(STATUS "Enabling workaround for libssl 1.1+ to set shared mutex attribute")
                set(LIBSSL_SET_MUTEX_SHARED ON)
            endif()
        endif()
    endif()
    
endif()

# -----------------------
#  Locking mechanism macro
# -----------------------

option(FAST_LOCK "Use fast locking" ON)
# Fast-lock not available for all platforms like mips
# Check the system processor
if(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|x86_64|sparc64|sparc|arm6|arm7|ppc|ppc64|alpha|mips2|mips64")
  set(USE_FAST_LOCK YES)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
  set(USE_FAST_LOCK YES)
  target_compile_definitions(common INTERFACE NOSMP) # memory barriers not implemented for arm
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips")
  set(USE_FAST_LOCK NO)
  target_compile_definitions(common INTERFACE MIPS_HAS_LLSC) # likely
  target_compile_definitions(common INTERFACE DNOSMP) # very likely
endif()

# Add definitions if USE_FAST_LOCK is YES
if(USE_FAST_LOCK)
  target_compile_definitions(common INTERFACE FAST_LOCK ADAPTIVE_WAIT ADAPTIVE_WAIT_LOOPS=1024)
endif()


# List of locking methods in option
set(locking_methods FAST_LOCK USE_FUTEX USE_PTHREAD_MUTEX USE_POSIX_SEM USE_SYSV_SEM)
set(LOCK_METHOD USE_FUTEX CACHE STRING "Locking method to use")
set_property(CACHE LOCK_METHOD PROPERTY STRINGS ${locking_methods})

# set(LOCKING_DEFINITION "${locking_method}")
message(STATUS "Locking method: ${LOCK_METHOD}")

if(${CMAKE_VERBOSE_MAKEFILE})
  message(STATUS "normal Makefile.defs exec")
endif()

include(compiler-specific.cmake)
include(os-specific.cmake)

target_compile_definitions(common INTERFACE 
    NAME="${MAIN_NAME}"
    VERSION="${RELEASE}"
    ARCH="${CMAKE_HOST_SYSTEM_PROCESSOR}"
    OS=${OS}
    OS_QUOTED="${OS}"
    COMPILER="${CMAKE_C_COMPILER_VERSION}"
    ${HOST_ARCH}
    VERSIONVAL=${VERSIONVAL}
    CFG_DIR=${CFG_NAME}
    FAST_LOCK 
    ${LOCK_METHOD}
    USE_TCP
    # CC_GCC_LIKE_ASM
    # $<$<CXX_COMPILER_ID:MSVC>:/Wall>
    # $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)
