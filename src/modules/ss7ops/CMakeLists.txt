file(GLOB SRC_FILES "*.c")

add_library(ss7ops SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(ss7ops PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ss7ops/=")
