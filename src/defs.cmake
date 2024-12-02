# Quite analogous to the Makefile.defs file This file is used to define the
# common flags and options for the project The flags are defined as INTERFACE
# properties of the common library The flags are then used by the other
# libraries and executables
cmake_minimum_required(VERSION 3.10)

add_library(common_modules INTERFACE)
add_library(common INTERFACE)

message(STATUS "CMAKE_C_COMPILER_VERSION: ${CMAKE_C_COMPILER_VERSION}")

set(OS ${CMAKE_SYSTEM_NAME})
message(STATUS "OS: ${OS}")

set(OSREL ${CMAKE_SYSTEM_VERSION})
message(STATUS "OS version: ${OSREL}")
# set(HOST_ARCH "__CPU_${CMAKE_HOST_SYSTEM_PROCESSOR}")
set(TARGET_ARCH "__CPU_${CMAKE_SYSTEM_PROCESSOR}")

message(STATUS "Host Processor: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
# message(STATUS "Processor compile definition: ${HOST_ARCH}")
message(STATUS "Target Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Target Processor compile definition: ${TARGET_ARCH}")

# TODO Check if target arch is supported if(NOT TARGET_ARCH IN_LIST
# supported_archs) message(FATAL_ERROR "Target architecture not supported")
# endif()

# Flavor of the project flavour: sip-router, ser or kamailio This is used to
# define the MAIN_NAME flag TODO: Kamailio only
set(flavours kamailio)
set(FLAVOUR
    "kamailio"
    CACHE STRING "Flavour of the project"
)
set_property(CACHE FLAVOUR PROPERTY STRINGS ${flavours})

# Verbose option (for debugging purposes) (was quiet in Makefile.defs) Probably
# not needed in CMake and can be removed Use the -DCMAKE_VERBOSE_MAKEFILE=ON
# option to enable verbose mode
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

option(NO_KQUEUE "No kqueue support" OFF)
option(NO_SELECT "No select support" OFF)
option(NO_EPOLL "No epoll support" OFF)
option(NO_SIGIO_RT "No poll support" OFF)
option(NO_DEV_POLL "No /dev/poll support" OFF)

option(USE_TCP "Use TCP" ON)
option(USE_TLS "Use TLS" ON)
option(USE_NAPTR "Use NAPTR" ON)
option(USE_DNS_CACHE "Use DNS cache" ON)

# Not strictly required to build
option(USE_SCTP "Use SCTP" ON)
option(DISABLE_NAGLE "Disable Nagle algorithm" ON)
option(USE_MCAST "Use  " ON)
option(DNS_IP_HACK "Use DNS IP hack" ON)
option(SHM_MMAP "Use mmap for shared memory" ON)

option(PKG_MALLOC "Use package memory" ON)
option(MEM_JOIN_FREE "Use mem_join_free" ON)
option(F_MALLOC "Use f_malloc" ON)
option(Q_MALLOC "Use q_malloc" ON)
option(TLSF_MALLOC "Use tlsf_malloc" ON)
option(MALLOC_STATS "Use malloc stats" ON)
option(DBG_SR_MEMORY "Use memory debugging system" ON)

option(USE_DNS_FAILOVER "Use DNS failover" ON)
option(USE_DST_BLOCKLIST "Use destination blacklist" ON)
option(HAVE_RESOLV_RES "Have resolv_res" ON)

option(KSR_PTHREAD_MUTEX_SHARED "Use shared mutex for TLS" ON)
option(FMSTATS "Fast memory statistics" ON)
option(STATISTICS "Statistics" ON)
# if(${MEMPKG}) target_compile_definitions(common INTERFACE PKG_MALLOC) else()
# if(${MEMDBGSYS}) target_compile_definitions(common INTERFACE DDBG_SYS_MEMORY)
# endif() endif()

# -----------------------
# TLS support
# -----------------------
# TLS support
option(TLS_HOOKS "TLS hooks support" ON)
option(CORE_TLS "CORE_TLS" OFF)
# set(CORE_TLS "" CACHE STRING "CORE_TLS") set(TLS_HOOKS ON CACHE BOOL "TLS
# hooks support")

if(${CORE_TLS})
  set(RELEASE "${RELEASE}-tls")
  set(TLS_HOOKS OFF)
else()
  set(TLS_HOOKS ON)
endif()

set(LIBSSL_SET_MUTEX_SHARED
    ON
    CACHE BOOL
          "enable workaround for libssl 1.1+ to set shared mutex attribute"
)
if(NOT ${LIBSSL_SET_MUTEX_SHARED})
  message(
    STATUS
      "Checking if can enable workaround for libssl 1.1+ to set shared mutex attribute"
  )
  if(NOT DEFINED CMAKE_CROSSCOMPILING OR NOT ${CMAKE_CROSSCOMPILING})
    message(STATUS "Checking for OpenSSL 1.1.0")
    find_package(OpenSSL 1.1.0)
    if(OPENSSL_FOUND)
      message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
      if(${OPENSSL_VERSION} VERSION_GREATER_EQUAL "1.1.0")
        message(
          STATUS
            "Enabling workaround for libssl 1.1+ to set shared mutex attribute"
        )
        set(LIBSSL_SET_MUTEX_SHARED ON)
      endif()
    endif()
  endif()

endif()

# -----------------------
# Locking mechanism macro
# -----------------------

option(FAST_LOCK "Use fast locking" ON)
# Fast-lock not available for all platforms like mips Check the system processor
if(CMAKE_SYSTEM_PROCESSOR MATCHES
   "i386|x86_64|sparc64|sparc|arm6|arm7|ppc|ppc64|alpha|mips2|mips64"
)
  set(USE_FAST_LOCK YES)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
  set(USE_FAST_LOCK YES)
  target_compile_definitions(common INTERFACE NOSMP) # memory barriers not
                                                     # implemented for arm
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips")
  set(USE_FAST_LOCK NO)
  target_compile_definitions(common INTERFACE MIPS_HAS_LLSC) # likely
  target_compile_definitions(common INTERFACE NOSMP) # very likely
endif()

# Add definitions if USE_FAST_LOCK is YES
if(USE_FAST_LOCK)
  target_compile_definitions(
    common INTERFACE FAST_LOCK ADAPTIVE_WAIT ADAPTIVE_WAIT_LOOPS=1024
  )
endif()
message(STATUS "Fast lock: ${USE_FAST_LOCK}")

# List of locking methods in option
set(locking_methods USE_FUTEX USE_PTHREAD_MUTEX USE_POSIX_SEM USE_SYSV_SEM)
set(LOCK_METHOD
    USE_FUTEX
    CACHE STRING "Locking method to use"
)
set_property(CACHE LOCK_METHOD PROPERTY STRINGS ${locking_methods})

# set(LOCKING_DEFINITION "${locking_method}")
message(STATUS "Locking method: ${LOCK_METHOD}")

# -----------------------
# Setting all the flags from options
if(USE_TCP)
  target_compile_definitions(common INTERFACE USE_TCP)
endif()

if(USE_TLS)
  target_compile_definitions(common INTERFACE USE_TLS)
endif()

if(TLS_HOOKS)
  target_compile_definitions(common INTERFACE TLS_HOOKS)
endif()

if(USE_NAPTR)
  target_compile_definitions(common INTERFACE USE_NAPTR)
endif()

if(USE_DNS_CACHE)
  target_compile_definitions(common INTERFACE USE_DNS_CACHE)
endif()

if(F_MALLOC)
  target_compile_definitions(common INTERFACE F_MALLOC)
endif()

if(Q_MALLOC)
  target_compile_definitions(common INTERFACE Q_MALLOC)
endif()

if(TLSF_MALLOC)
  target_compile_definitions(common INTERFACE TLSF_MALLOC)
endif()

if(MALLOC_STATS)
  target_compile_definitions(common INTERFACE MALLOC_STATS)
endif()

if(DBG_SR_MEMORY)
  target_compile_definitions(common INTERFACE DBG_SR_MEMORY)
endif()

if(USE_DNS_FAILOVER)
  target_compile_definitions(common INTERFACE USE_DNS_FAILOVER)
endif()

if(USE_DST_BLOCKLIST)
  target_compile_definitions(common INTERFACE USE_DST_BLOCKLIST)
endif()

if(HAVE_RESOLV_RES)
  target_compile_definitions(common INTERFACE HAVE_RESOLV_RES)
endif()

if(USE_MCAST)
  target_compile_definitions(common INTERFACE USE_MCAST)
endif()

if(DISABLE_NAGLE)
  target_compile_definitions(common INTERFACE DISABLE_NAGLE)
endif()

if(DNS_IP_HACK)
  target_compile_definitions(common INTERFACE DNS_IP_HACK)
endif()

if(SHM_MMAP)
  target_compile_definitions(common INTERFACE SHM_MMAP)
endif()

if(PKG_MALLOC)
  target_compile_definitions(common INTERFACE PKG_MALLOC)
endif()

if(NO_KQUEUE)
  target_compile_definitions(common INTERFACE NO_KQUEUE)
endif()

if(NO_SELECT)
  target_compile_definitions(common INTERFACE NO_SELECT)
endif()

if(NO_EPOLL)
  target_compile_definitions(common INTERFACE NO_EPOLL)
endif()

if(NO_SIGIO_RT)
  target_compile_definitions(common INTERFACE NO_SIGIO_RT)
endif()

if(NO_DEV_POLL)
  target_compile_definitions(common INTERFACE NO_DEV_POLL)
endif()

# if(USE_SCTP) target_compile_definitions(common INTERFACE USE_SCTP) endif()

if(RAW_SOCKS)
  target_compile_definitions(common INTERFACE RAW_SOCKS)
endif()

if(KSR_PTHREAD_MUTEX_SHARED)
  target_compile_definitions(common INTERFACE KSR_PTHREAD_MUTEX_SHARED)
endif()

if(FMSTATS)
  target_compile_definitions(common INTERFACE FMSTATS)
endif()

if(KMSTATS)
  target_compile_definitions(common INTERFACE KMSTATS)
endif()

include(compiler-specific.cmake)
include(os-specific.cmake)

string(TOLOWER ${OS} OS_LOWER)
target_compile_definitions(
  common
  INTERFACE
    NAME="${MAIN_NAME}"
    VERSION="${RELEASE}"
    ARCH="${CMAKE_HOST_SYSTEM_PROCESSOR}"
    OS=${OS}
    OS_QUOTED="${OS}"
    COMPILER="${CMAKE_C_COMPILER_VERSION}"
    # ${HOST_ARCH}
    ${TARGET_ARCH}
    __OS_${OS_LOWER}
    VERSIONVAL=${VERSIONVAL}
    CFG_DIR="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/${CFG_NAME}/"
    SHARE_DIR="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${MAIN_NAME}/"
    RUN_DIR="/${CMAKE_INSTALL_LOCALSTATEDIR}/run/${MAIN_NAME}"
    ${LOCK_METHOD}
    # Module stuff?
    PIC
    # TODO: We can use the generator expression to define extra flags instead of
    # checking the options each time
    $<$<BOOL:${USE_SCTP}>:USE_SCTP>
    $<$<BOOL:${STATISTICS}>:STATISTICS>
)
target_compile_options(common_modules INTERFACE -fPIC)
