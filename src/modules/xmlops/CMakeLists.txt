find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xmlops SHARED ${SRC_FILES})
target_link_libraries(xmlops PRIVATE LibXml2::LibXml2)
