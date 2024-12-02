cmake_minimum_required(VERSION 3.10)

project(kamailio_lib_trie VERSION 1.0.0)

file(GLOB SRC_FILES "*.c")

add_library(lib_trie SHARED ${SRC_FILES})
target_link_libraries(lib_trie common kamailio)

# TODO: do we need version control?
set_target_properties(
  lib_trie PROPERTIES VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
                      OUTPUT_NAME trie
)
set_target_properties(lib_trie PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(trie ALIAS lib_trie)
