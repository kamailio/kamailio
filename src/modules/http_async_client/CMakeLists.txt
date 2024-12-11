file(GLOB SRC_FILES "*.c")

find_package(CURL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EVENT REQUIRED IMPORTED_TARGET libevent)
add_library(event::event ALIAS PkgConfig::EVENT)

add_library(http_async_client SHARED ${SRC_FILES})
target_link_libraries(http_async_client PRIVATE CURL::libcurl event::event)

target_compile_options(http_async_client PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/http_async_client/=")
