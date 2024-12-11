file(GLOB SRC_FILES "*.c")

add_library(siptrace SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(siptrace PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/siptrace/=")
