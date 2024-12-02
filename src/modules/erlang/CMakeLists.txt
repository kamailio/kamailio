cmake_minimum_required(VERSION 3.10)

project(erlang)

# erlang-dev
find_package(Erlang REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(erlang SHARED ${SRC_FILES})
target_link_libraries(erlang PRIVATE kamailio Erlang::EI)
