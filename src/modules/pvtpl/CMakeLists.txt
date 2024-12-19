file(GLOB SRC_FILES "*.c")

add_library(pvtpl SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(pvtpl PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pvtpl/=")
