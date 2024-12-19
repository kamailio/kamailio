file(GLOB SRC_FILES "*.c")

add_library(timer SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(timer PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/timer/=")
