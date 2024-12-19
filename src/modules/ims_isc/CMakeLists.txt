find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(ims_isc SHARED ${SRC_FILES})
target_link_libraries(ims_isc PRIVATE LibXml2::LibXml2)

target_compile_options(
  ims_isc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ims_isc/=")
set_target_properties(ims_isc PROPERTIES SUFFIX ".so")
