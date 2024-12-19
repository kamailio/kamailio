file(GLOB SRC_FILES "*.c")

add_library(ims_usrloc_scscf SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  ims_usrloc_scscf
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ims_usrloc_scscf/=")
set_target_properties(ims_usrloc_scscf PROPERTIES SUFFIX ".so")
