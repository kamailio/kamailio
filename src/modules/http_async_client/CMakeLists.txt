cmake_minimum_required(VERSION 3.10)

project(http_async_client)

file(GLOB SRC_FILES "*.c")

find_package(CURL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EVENT REQUIRED IMPORTED_TARGET libevent)
add_library(event::event ALIAS PkgConfig::EVENT)

add_library(http_async_client SHARED ${SRC_FILES})
target_link_libraries(
  http_async_client PRIVATE kamailio CURL::libcurl event::event
)
