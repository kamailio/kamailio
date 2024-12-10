file(GLOB SRC_FILES "*.c")

find_package(LibXml2 REQUIRED)

add_library(cdp SHARED ${SRC_FILES})
target_link_libraries(cdp PRIVATE LibXml2::LibXml2)
