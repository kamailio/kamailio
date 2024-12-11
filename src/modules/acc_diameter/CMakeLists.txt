file(GLOB SRC_FILES "*.c")

add_library(acc_diameter SHARED ${SRC_FILES})

target_compile_options(acc_diameter PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/acc_diameter/=")
