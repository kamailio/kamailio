find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")
target_sources(kamailio PUBLIC ${SRC_FILES})
