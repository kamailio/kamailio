find_package(LibXml2 REQUIRED)

# TODO: Only for darwin?
find_package(ICU QUIET COMPONENTS uc)

file(GLOB SRC_FILES "*.c")

add_library(rls SHARED ${SRC_FILES})

target_link_libraries(rls PRIVATE LibXml2::LibXml2 ICU::uc)
