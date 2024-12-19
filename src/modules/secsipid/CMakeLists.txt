file(GLOB SRC_FILES "*.c")

add_library(secsipid SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(secsipid PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/secsipid/=")
