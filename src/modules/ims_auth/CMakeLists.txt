find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(ims_auth SHARED ${SRC_FILES})
target_link_libraries(ims_auth PRIVATE LibXml2::LibXml2)

target_compile_options(ims_auth PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ims_auth/=")
