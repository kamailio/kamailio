file(GLOB auth_SRC "*.c")

add_library(auth SHARED ${auth_SRC})
# target_link_libraries(.*PRIVATE)
target_compile_definitions(auth PRIVATE USE_NC USE_OT_NONCE)
