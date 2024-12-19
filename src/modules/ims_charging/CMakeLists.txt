file(GLOB SRC_FILES "*.c")

add_library(ims_charging SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(ims_charging PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ims_charging/=")
