find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xcap_server SHARED ${SRC_FILES})

target_link_libraries(xcap_server PRIVATE LibXml2::LibXml2)
