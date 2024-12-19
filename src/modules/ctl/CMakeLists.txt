file(GLOB SRC_FILES "*.c")

add_library(ctl SHARED ${SRC_FILES})
# TODO: Should this be picked by user defined or hardcoded?
target_compile_definitions(ctl PRIVATE USE_FIFO CTL_SYSTEM_MALLOC)
# target_link_libraries(.*PRIVATE)

# TODO: Also install kamcmd binary

target_compile_options(
  ctl PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ctl/=")
set_target_properties(ctl PROPERTIES SUFFIX ".so")
