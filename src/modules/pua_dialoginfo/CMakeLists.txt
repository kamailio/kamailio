find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_dialoginfo SHARED ${SRC_FILES})
target_link_libraries(pua_dialoginfo PRIVATE LibXml2::LibXml2)
