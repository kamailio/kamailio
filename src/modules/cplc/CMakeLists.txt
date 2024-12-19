file(GLOB SRC_FILES "*.c")

find_package(LibXml2 REQUIRED)

add_library(cplc SHARED ${SRC_FILES})
target_link_libraries(cplc PRIVATE LibXml2::LibXml2)
