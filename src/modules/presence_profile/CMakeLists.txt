file(GLOB SRC_FILES "*.c")

add_library(presence_profile SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  presence_profile
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/presence_profile/=")
set_target_properties(presence_profile PROPERTIES SUFFIX ".so")
