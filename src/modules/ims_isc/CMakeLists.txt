find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(ims_isc SHARED ${SRC_FILES})
target_link_libraries(ims_isc PRIVATE LibXml2::LibXml2)
