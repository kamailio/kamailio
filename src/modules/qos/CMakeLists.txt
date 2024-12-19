file(GLOB SRC_FILES "*.c")

add_library(qos SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(qos PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/qos/=")
