# Quite analogous to the Makefile.defs file
# This file is used to define the common flags and options for the project
# The flags are defined as INTERFACE properties of the common library
# The flags are then used by the other libraries and executables
cmake_minimum_required(VERSION 3.10)

# Define the common flags and options for GCC

# Define the flags for the C compiler
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_compile_definitions(common INTERFACE CC_GCC_LIKE_ASM)
  target_compile_options(common INTERFACE -Wall -funroll-loops  -Wcast-align)


endif()