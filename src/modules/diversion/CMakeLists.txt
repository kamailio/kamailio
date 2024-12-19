file(GLOB SRC_FILES "*.c")

add_library(diversion SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(diversion PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/diversion/=")
