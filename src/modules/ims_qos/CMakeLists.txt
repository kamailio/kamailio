file(GLOB SRC_FILES "*.c")

add_library(ims_qos SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  ims_qos PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ims_qos/=")
set_target_properties(ims_qos PROPERTIES SUFFIX ".so")
