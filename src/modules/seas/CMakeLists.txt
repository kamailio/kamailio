file(GLOB SRC_FILES "*.c")

add_library(seas SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(seas PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/seas/=")
