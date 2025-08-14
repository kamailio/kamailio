cmake_minimum_required(VERSION 3.10)

project(lock_contention_tests)
get_filename_component(KAMAILIO_SRC_CORE_DIR "../../../src/core" ABSOLUTE)
message(STATUS "SOURCE DIR: ${KAMAILIO_SRC_CORE_DIR}")

add_executable(test-contention-sysfutex futex-test-kamailio.c)
target_compile_definitions(test-contention-sysfutex PRIVATE SYS_FUTEX)
target_compile_definitions(
  test-contention-sysfutex PRIVATE HAVE_SCHED_YIELD _DEFAULT_SOURCE __CPU_x86_64
)
target_link_libraries(test-contention-sysfutex PRIVATE pthread)

add_executable(test-contention-fastlock futex-test-kamailio.c)
target_include_directories(test-contention-fastlock PRIVATE ${KAMAILIO_SRC_CORE_DIR})
target_compile_definitions(test-contention-fastlock PRIVATE FAST_LOCK)
target_compile_definitions(
  test-contention-fastlock PRIVATE HAVE_SCHED_YIELD _DEFAULT_SOURCE __CPU_x86_64
)
target_link_libraries(test-contention-fastlock PRIVATE pthread)

add_executable(test-contention-kamailio-futex futex-test-kamailio.c)
target_include_directories(test-contention-kamailio-futex PRIVATE ${KAMAILIO_SRC_CORE_DIR})
target_compile_definitions(test-contention-kamailio-futex PRIVATE KAMAILIO_FUTEX)
target_compile_definitions(
  test-contention-kamailio-futex PRIVATE HAVE_SCHED_YIELD _DEFAULT_SOURCE __CPU_x86_64
                                         ADAPTIVE_WAIT ADAPTIVE_WAIT_LOOPS=1024 CC_GCC_LIKE_ASM
)
target_link_libraries(test-contention-kamailio-futex PRIVATE pthread)

add_executable(test-contention-kamailio-futex-no-adaptive futex-test-kamailio.c)
target_include_directories(
  test-contention-kamailio-futex-no-adaptive PRIVATE ${KAMAILIO_SRC_CORE_DIR}
)
target_compile_definitions(test-contention-kamailio-futex-no-adaptive PRIVATE KAMAILIO_FUTEX)
target_compile_definitions(
  test-contention-kamailio-futex-no-adaptive PRIVATE HAVE_SCHED_YIELD _DEFAULT_SOURCE __CPU_x86_64
                                                     CC_GCC_LIKE_ASM
)
target_link_libraries(test-contention-kamailio-futex-no-adaptive PRIVATE pthread)
