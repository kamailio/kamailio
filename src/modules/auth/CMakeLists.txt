cmake_minimum_required(VERSION 3.10)

project(auth)

file(GLOB auth_SRC "*.c")

add_library(auth SHARED ${auth_SRC})
target_link_libraries(auth common common_modules kamailio)
target_compile_definitions(auth PRIVATE USE_NC USE_OT_NONCE)
