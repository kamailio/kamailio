file(GLOB SRC_FILES "*.c")

add_library(cfg_rpc SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

# TODO: Also install kamcmd binary

target_compile_options(
  cfg_rpc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/cfg_rpc/=")
set_target_properties(cfg_rpc PROPERTIES SUFFIX ".so")
