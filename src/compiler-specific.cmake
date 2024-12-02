# Quite analogous to the Makefile.defs file This file is used to define the
# common flags and options for the project The flags are defined as INTERFACE
# properties of the common library The flags are then used by the other
# libraries and executables
cmake_minimum_required(VERSION 3.10)

# Define the common flags and options for GCC
option(PROFILE "Enable profiling" OFF)

# Define the flags for the C compiler
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")

  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_definitions(common INTERFACE CC_GCC_LIKE_ASM)

    target_compile_options(
      common INTERFACE -O0
                       # <$<$<BOOL:${PROFILE}>:-pg>
    )

    target_compile_options(common INTERFACE -Wall -funroll-loops -Wcast-align)

    # If GCC version is greater than 4.2.0, enable the following flags
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.2.0)
      target_compile_options(
        common INTERFACE -m64 -minline-all-stringops -falign-loops
                         -ftree-vectorize -fno-strict-overflow -mtune=generic
      )
    endif()

  endif()

elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")

  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_definitions(common INTERFACE CC_GCC_LIKE_ASM NOSMP)

    # target_compile_options(common INTERFACE -O0 # <$<$<BOOL:${PROFILE}>:-pg> )

    # target_compile_options( common INTERFACE -marm -march=armv5t
    # -funroll-loops -fsigned-char )

    # # If GCC version is greater than 4.2.0, enable the following flags
    # if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.2.0) target_compile_options(
    # common INTERFACE -ftree-vectorize -fno-strict-overflow ) endif()

  endif()
endif()
