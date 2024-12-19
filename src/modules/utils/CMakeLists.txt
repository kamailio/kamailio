find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(utils SHARED ${SRC_FILES})

target_link_libraries(utils PRIVATE LibXml2::LibXml2)
