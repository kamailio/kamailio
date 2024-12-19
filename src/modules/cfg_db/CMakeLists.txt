file(GLOB SRC_FILES "*.c")

add_library(cfg_db SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  cfg_db PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/cfg_db/=")
set_target_properties(cfg_db PROPERTIES SUFFIX ".so")
