file(GLOB SRC_FILES "*.c")

add_library(file_out SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(file_out PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/file_out/=")
