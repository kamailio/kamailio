file(GLOB SRC_FILES "*.c")

find_package(LibXml2 REQUIRED)

add_library(ims_registrar_scscf SHARED ${SRC_FILES})
target_link_libraries(ims_registrar_scscf PRIVATE LibXml2::LibXml2)

target_compile_options(ims_registrar_scscf PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ims_registrar_scscf/=")
