find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_xmpp SHARED ${SRC_FILES})
target_link_libraries(pua_xmpp PRIVATE LibXml2::LibXml2)
