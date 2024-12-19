find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_reginfo SHARED ${SRC_FILES})
target_link_libraries(pua_reginfo PRIVATE LibXml2::LibXml2)

target_compile_options(pua_reginfo PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pua_reginfo/=")
