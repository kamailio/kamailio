# option(USE_FAST_LOCK "Use fast locking if available" ON)

# Locking method selection
# Priority (auto): FUTEX > FAST_LOCK > PTHREAD_MUTEX > POSIX_SEM > SYSV_SEM

# Public cache variable (user may force one); AUTO means detect
set(locking_methods "AUTO" "FAST_LOCK" "FUTEX" "PTHREAD_MUTEX" "POSIX_SEM" "SYSV_SEM")
set(LOCK_METHOD
    "AUTO"
    CACHE
      STRING
      "Locking method to use (AUTO, or one of: FUTEX FAST_LOCK PTHREAD_MUTEX POSIX_SEM SYSV_SEM)"
)
# List of locking methods in option
set_property(CACHE LOCK_METHOD PROPERTY STRINGS ${locking_methods})
mark_as_advanced(LOCK_METHOD)

# Set default locking method if not set
if(NOT LOCK_METHOD)
  message(STATUS "No locking method specified, using default: AUTO")
  set(LOCK_METHOD "AUTO")
endif()

# Validate the selected locking method
if(NOT LOCK_METHOD IN_LIST locking_methods)
  message(
    FATAL_ERROR
      "Invalid locking method selected: ${LOCK_METHOD}. Methods available are: ${locking_methods}"
  )
endif()

# Availability checks
# FUTEX: linux/futex.h
find_path(FUTEX_HEADER_DIR linux/futex.h)
if(FUTEX_HEADER_DIR)
  set(_HAVE_FUTEX TRUE)
  # Unfortunately, arm64 and aarch64 does not have yet implemented the nessecary defines
  # and operations in atomic_arm.h needed for the futexlock.h
  if("${TARGET_ARCH}" MATCHES "aarch64$|arm64$")
    set(_HAVE_FUTEX FALSE)
  endif()
else()
  set(_HAVE_FUTEX FALSE)
endif()

# PTHREAD: pthread.h (assume present on typical systems)
find_path(PTHREAD_HEADER_DIR pthread.h)
if(PTHREAD_HEADER_DIR)
  set(_HAVE_PTHREAD TRUE)
else()
  set(_HAVE_PTHREAD FALSE)
endif()

# POSIX SEM: semaphore.h
find_path(SEM_HEADER_DIR semaphore.h)
if(SEM_HEADER_DIR)
  set(_HAVE_POSIX_SEM TRUE)
else()
  set(_HAVE_POSIX_SEM FALSE)
endif()

# SYSV SEM: sys/sem.h
find_path(SYSV_SEM_HEADER_DIR sys/sem.h)
if(SYSV_SEM_HEADER_DIR)
  set(_HAVE_SYSV_SEM TRUE)
else()
  set(_HAVE_SYSV_SEM FALSE)
endif()

# check fast-lock arch support
set(_FAST_LOCK_ARCH FALSE)
if("${TARGET_ARCH}" MATCHES
   "i386$|x86_64$|aarch64$|arm6$|arm7$|ppc$|ppc64$|sparc64$|sparc$|alpha$|mips2$|mips64$"
)
  set(_HAVE_FAST_LOCK TRUE)
elseif("${TARGET_ARCH}" MATCHES "mips$")
  # explicitly unsupported (old code added extra defs)
  set(_HAVE_FAST_LOCK FALSE)
endif()

message(
  STATUS
    "Locking Methods for this platform: FUTEX=${_HAVE_FUTEX} FAST_LOCK=${_HAVE_FAST_LOCK} PTHREAD=${_HAVE_PTHREAD} POSIX_SEM=${_HAVE_POSIX_SEM} SYSV_SEM=${_HAVE_SYSV_SEM}"
)

# Final locking method selection logic
set(_SELECTED_LOCK_METHOD "")

if("${LOCK_METHOD}" STREQUAL "AUTO")
  # Priority according to code in lock_ops.h
  if(_HAVE_FUTEX)
    set(_SELECTED_LOCK_METHOD "FUTEX")
  elseif(_HAVE_FAST_LOCK)
    set(_SELECTED_LOCK_METHOD "FAST_LOCK")
  elseif(_HAVE_PTHREAD)
    set(_SELECTED_LOCK_METHOD "PTHREAD_MUTEX")
  elseif(_HAVE_POSIX_SEM)
    set(_SELECTED_LOCK_METHOD "POSIX_SEM")
  elseif(_HAVE_SYSV_SEM)
    set(_SELECTED_LOCK_METHOD "SYSV_SEM")
  else()
    message(FATAL_ERROR "No supported locking method found for this platform.")
  endif()
else()
  set(_SELECTED_LOCK_METHOD "${LOCK_METHOD}")
endif()

message(STATUS "Selected locking method: ${_SELECTED_LOCK_METHOD}")

# Set compile definitions based on the selected method
if("${_SELECTED_LOCK_METHOD}" STREQUAL "FUTEX")
  target_compile_definitions(common INTERFACE USE_FUTEX)
elseif("${_SELECTED_LOCK_METHOD}" STREQUAL "FAST_LOCK")
  target_compile_definitions(common INTERFACE FAST_LOCK ADAPTIVE_WAIT ADAPTIVE_WAIT_LOOPS=1024)
  if("${TARGET_ARCH}" MATCHES "mips$")
    # Add special definitions for mips + FAST_LOCK
    target_compile_definitions(common INTERFACE MIPS_HAS_LLSC) # likely
    target_compile_definitions(common INTERFACE NOSMP) # very likely
  elseif("${TARGET_ARCH}" MATCHES "arm$|aarch64$")
    target_compile_definitions(common INTERFACE NOSMP) # memory barriers not implemented for arm
  endif()
elseif("${_SELECTED_LOCK_METHOD}" STREQUAL "PTHREAD_MUTEX")
  target_compile_definitions(common INTERFACE USE_PTHREAD_MUTEX)
  target_link_libraries(common INTERFACE pthread)
elseif("${_SELECTED_LOCK_METHOD}" STREQUAL "POSIX_SEM")
  target_compile_definitions(common INTERFACE USE_POSIX_SEM)
  target_link_libraries(common INTERFACE pthread)
elseif("${_SELECTED_LOCK_METHOD}" STREQUAL "SYSV_SEM")
  target_compile_definitions(common INTERFACE USE_SYSV_SEM)
else()
  message(FATAL_ERROR "Unknown locking method: ${_SELECTED_LOCK_METHOD}")
endif()

# Cache the final locking method for use in other parts of the build
# This is an internal cache variable, not meant for user modification
set(LOCK_METHOD_FINAL
    "${_SELECTED_LOCK_METHOD}"
    CACHE INTERNAL "Final locking method selected"
)
