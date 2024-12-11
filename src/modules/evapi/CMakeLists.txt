find_package(Libev REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(evapi SHARED ${SRC_FILES})
target_link_libraries(evapi PRIVATE Libev::Libev)

target_compile_options(evapi PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/evapi/=")
