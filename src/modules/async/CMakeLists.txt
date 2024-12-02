cmake_minimum_required(VERSION 3.10)

project(async)

# Define the library target
add_library(async SHARED async_mod.c async_sleep.c)
target_link_libraries(async common)

# Set the output name of the library to match the Makefile's output
set_target_properties(async PROPERTIES OUTPUT_NAME "async")