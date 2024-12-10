find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xhttp_pi SHARED ${SRC_FILES})

target_link_libraries(xhttp_pi PRIVATE LibXml2::LibXml2)
