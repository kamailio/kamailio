find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(presence_dfks SHARED ${SRC_FILES})
target_link_libraries(presence_dfks PRIVATE LibXml2::LibXml2)
