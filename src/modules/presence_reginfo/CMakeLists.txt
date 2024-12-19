find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(presence_reginfo SHARED ${SRC_FILES})
target_link_libraries(presence_reginfo PRIVATE LibXml2::LibXml2)

target_compile_options(presence_reginfo PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/presence_reginfo/=")
