find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(ims_icscf SHARED ${SRC_FILES})

target_link_libraries(ims_icscf PRIVATE LibXml2::LibXml2)

target_compile_options(ims_icscf PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ims_icscf/=")
