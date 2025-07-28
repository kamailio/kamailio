# Quite analogous to the Makefile.defs file This file is used to define the
# common flags and options for the project The flags are defined as INTERFACE
# properties of the common library The flags are then used by the other
# libraries and executables

include(CMakeDependentOption) # cmake_dependent_option

add_library(common INTERFACE)

# This interface is populated by common and some extra module specific flags
# See end of file
add_library(common_modules INTERFACE)

# This interface is populated by common compiler flags and some os specific
# flags. See each os specific file for more details.
add_library(common_utils INTERFACE)

#
set(OS ${CMAKE_SYSTEM_NAME})
message(STATUS "OS: ${OS}")

set(OSREL ${CMAKE_SYSTEM_VERSION})
message(STATUS "OS version: ${OSREL}")
# set(HOST_ARCH "__CPU_${CMAKE_HOST_SYSTEM_PROCESSOR}")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i486|i586|i686")
  set(TARGET_ARCH "i386")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
  set(TARGET_ARCH "x86_64")
else()
  set(TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "Host Processor: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
# message(STATUS "Processor compile definition: ${HOST_ARCH}")
message(STATUS "Target Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Target Processor Alias: ${TARGET_ARCH}")

# TODO Check if target arch is supported if(NOT TARGET_ARCH IN_LIST
# supported_archs) message(FATAL_ERROR "Target architecture not supported")
# endif()

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
option(RAW_SOCKS "Raw sockets support" ON)

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
option(USE_MCAST "Use Multicast" ON)
option(DNS_IP_HACK "Use DNS IP hack" ON)
option(SHM_MMAP "Use mmap for shared memory" ON)

# memory managers and related debug mode
option(PKG_MALLOC "Use custom package memory manager (OFF will use system manager)" ON)
# Requires MEMPKG to be ON
option(MEMDBG "Use memory debugging system" ON)
cmake_dependent_option(MEMDBGSYS "Debug system memory manager" OFF "NOT PKG_MALLOC" OFF)

option(MEM_JOIN_FREE "Use mem_join_free" ON)
option(F_MALLOC "Use f_malloc" ON)
option(Q_MALLOC "Use q_malloc" ON)
# The following symbol is defined also when DBG_SR_MEMORY is defined
# in the respective q_malloc.h
# Same goes for fmalloc and tlsf malloc
# cmake_dependent_option(DBG_QM_MALLOC "Enable debugging info for q_malloc" OFF "Q_MALLOC" OFF)
option(TLSF_MALLOC "Use tlsf_malloc" ON)
option(MALLOC_STATS "Use malloc stats" ON)

option(USE_DNS_FAILOVER "Use DNS failover" ON)
option(USE_DST_BLOCKLIST "Use destination blacklist" ON)
option(HAVE_RESOLV_RES "Have resolv_res" ON)

option(KSR_PTHREAD_MUTEX_SHARED "Use shared mutex for TLS" ON)
option(STATISTICS "Statistics" ON)

# if(${MEMPKG})
#   target_compile_definitions(common INTERFACE PKG_MALLOC)
# else()
#   if(${MEMDBGSYS})
#     target_compile_definitions(common INTERFACE DDBG_SYS_MEMORY)
#   endif()
# endif()

# -----------------------
# TLS support
# -----------------------
option(TLS_HOOKS "TLS hooks support" ON)
option(CORE_TLS "CORE_TLS" OFF)
# set(CORE_TLS "" CACHE STRING "CORE_TLS")
# set(TLS_HOOKS
#     ON
#     CACHE BOOL "TLS hooks support"
# )

if(${CORE_TLS})
  set(RELEASE "${RELEASE}-tls")
  set(TLS_HOOKS OFF)
  target_compile_definitions(common INTERFACE CORE_TLS)
else()
  set(TLS_HOOKS ON)
endif()

set(LIBSSL_SET_MUTEX_SHARED
    ON
    CACHE BOOL "enable workaround for libssl 1.1+ to set shared mutex attribute"
)
if(NOT ${LIBSSL_SET_MUTEX_SHARED})
  message(STATUS "Checking if can enable workaround for libssl 1.1+ to set shared mutex attribute")

  # TODO: This can probably be reduced to a just a find_package(OpenSSL) call
  # and then check the version
  # If we are cross-compiling, cmake should search for library on the target
  # or both target/host
  if(NOT DEFINED CMAKE_CROSSCOMPILING OR NOT ${CMAKE_CROSSCOMPILING})
    message(STATUS "Checking for OpenSSL 1.1.0")
    find_package(OpenSSL 1.1.0)
    if(OPENSSL_FOUND)
      message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
      if(${OPENSSL_VERSION} VERSION_GREATER_EQUAL "1.1.0")
        message(STATUS "Enabling workaround for libssl 1.1+ to set shared mutex attribute")
        set(LIBSSL_SET_MUTEX_SHARED ON)
      endif()
    endif()
  endif()

endif()

# -----------------------
# Locking mechanism macro
# -----------------------

option(USE_FAST_LOCK "Use fast locking if available" ON)

# TODO: Discuss if we need to expose this to the user to choose between
# different locking methods

# set(locking_methods FAST_LOCK USE_FUTEX USE_PTHREAD_MUTEX USE_POSIX_SEM
#                     USE_SYSV_SEM)
# set(LOCK_METHOD
#     ""
#     CACHE STRING "Locking method to use. Fast-lock if available is default")
# # List of locking methods in option
# set_property(CACHE LOCK_METHOD PROPERTY STRINGS ${locking_methods})
# mark_as_advanced(LOCK_METHOD)

# Fast-lock not available for all platforms like mips
# Check the system processor type and set USE_FAST_LOCK accordingly
if(USE_FAST_LOCK)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES
     "i386|i486|i586|i686|x86_64|amd64|sparc64|sparc|ppc$|ppc64$|alpha|mips2|mips64"
  )
    set(USE_FAST_LOCK YES)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(USE_FAST_LOCK NO)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm6|arm7")
    set(USE_FAST_LOCK YES)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
    set(USE_FAST_LOCK YES)
    target_compile_definitions(common INTERFACE NOSMP) # memory barriers not
                                                       # implemented for arm
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips")
    set(USE_FAST_LOCK NO)
    target_compile_definitions(common INTERFACE MIPS_HAS_LLSC) # likely
    target_compile_definitions(common INTERFACE NOSMP) # very likely
  else()
    message(STATUS "Fast locking not available for this platform, disabling USE_FAST_LOCK")
    set(USE_FAST_LOCK NO)
  endif()
endif()

# Add definitions if USE_FAST_LOCK is YES
message(STATUS "Fast lock available: USE_FAST_LOCK=${USE_FAST_LOCK}")
if(USE_FAST_LOCK)
  # If fast lock is available, add the definitions for it, else each OS will
  # have its own locking method
  target_compile_definitions(common INTERFACE FAST_LOCK ADAPTIVE_WAIT ADAPTIVE_WAIT_LOOPS=1024)
endif()

# set(LOCKING_DEFINITION "${locking_method}")
# message(STATUS "Locking method: ${LOCK_METHOD}")

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
# See note above on defintion of option
# Same goes for fmalloc and tlsf malloc
# if(DBG_QM_MALLOC)
#   target_compile_definitions(common INTERFACE DBG_QM_MALLOC)
# endif()

if(TLSF_MALLOC)
  target_compile_definitions(common INTERFACE TLSF_MALLOC)
endif()

if(MALLOC_STATS)
  target_compile_definitions(common INTERFACE MALLOC_STATS)
endif()

if(MEMDBG)
  target_compile_definitions(common INTERFACE DBG_SR_MEMORY)
  if(MEMDBGSYS)
    target_compile_definitions(common INTERFACE DBG_SYS_MEMORY)
  endif()
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

include(${CMAKE_SOURCE_DIR}/cmake/compiler-specific.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/os-specific.cmake)

set(COMPILER_NAME ${CMAKE_C_COMPILER_ID})
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  set(COMPILER_NAME "gcc")
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  set(COMPILER_NAME "clang")
endif()
if(NOT DEFINED RUN_DIR)
  set(RUN_DIR "run/${MAIN_NAME}")
endif()
string(TOLOWER ${OS} OS_LOWER)

# This is a convenience mechanism to provide undeclared options.
# If you find an undecleared option in the code, please declare it
# as an option os cache variable and try to use that instead of this.
set(EXTRA_DEFS
    ""
    CACHE STRING "Extra preprocessor definitions (semicolon-separated string, e.g. FOO;BAR=1)"
)
if(EXTRA_DEFS)
  foreach(def ${EXTRA_DEFS})
    target_compile_definitions(common INTERFACE ${def})
  endforeach()
endif()

target_compile_definitions(
  common
  INTERFACE NAME="${MAIN_NAME}"
            VERSION="${RELEASE}"
            ARCH="${TARGET_ARCH}"
            OS=${OS}
            OS_QUOTED="${OS}"
            COMPILER="${COMPILER_NAME} ${CMAKE_C_COMPILER_VERSION}"
            # ${HOST_ARCH}
            __CPU_${TARGET_ARCH}
            __OS_${OS_LOWER}
            VERSIONVAL=${VERSIONVAL}
            CFG_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}/${CFG_NAME}/"
            SHARE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/${MAIN_NAME}/"
            # Absolute path this run is always /var/run/kamailio either for
            # local or system installs
            RUN_DIR="${RUN_PREFIX}/${RUN_DIR}"
            # Module stuff?
            # PIC
            # TODO: We can use the generator expression to define extra flags
            # instead of checking the options each time
            $<$<BOOL:${USE_SCTP}>:USE_SCTP>
            $<$<BOOL:${STATISTICS}>:STATISTICS>
)
target_link_libraries(common INTERFACE common_compiler_flags)

# ----------------------
# Common modules
# ---------------------
target_compile_options(common_modules INTERFACE -fPIC)
# TODO: Do we need all the option from common as well?
target_link_libraries(common_modules INTERFACE common)

# ----------------------
# Common utils
# ---------------------
target_link_libraries(common_utils INTERFACE common_compiler_flags)
