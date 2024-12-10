find_package(PkgConfig REQUIRED)
pkg_check_modules(libmnl REQUIRED IMPORTED_TARGET libmnl)
add_library(libmnl::libmnl ALIAS PkgConfig::libmnl)

file(GLOB SRC_FILES "*.c")

add_library(ims_ipsec_pcscf SHARED ${SRC_FILES})

target_link_libraries(ims_ipsec_pcscf PRIVATE libmnl::libmnl)
