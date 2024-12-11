file(GLOB SRC_FILES "*.c")

add_library(log_custom SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(log_custom PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/log_custom/=")
