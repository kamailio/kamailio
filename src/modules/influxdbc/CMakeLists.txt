file(GLOB SRC_FILES "*.c")

add_library(influxdbc SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(influxdbc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/influxdbc/=")
