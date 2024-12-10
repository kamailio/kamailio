# erlang-dev
find_package(Erlang REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(erlang SHARED ${SRC_FILES})
target_link_libraries(erlang PRIVATE Erlang::EI)
