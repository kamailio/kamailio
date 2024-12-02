cmake_minimum_required(VERSION 3.10)

project(auth)

file(GLOB auth_SRC
    "*.c"
)

# Define the library target
add_library(auth SHARED ${auth_SRC})
target_link_libraries(auth common)
target_compile_definitions(auth PRIVATE 
            USE_NC
            USE_OT_NONCE)

# Set the output name of the library to match the Makefile's output
set_target_properties(auth PROPERTIES OUTPUT_NAME "auth")