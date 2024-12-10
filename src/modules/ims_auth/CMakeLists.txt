find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(ims_auth SHARED ${SRC_FILES})
target_link_libraries(ims_auth PRIVATE LibXml2::LibXml2)
