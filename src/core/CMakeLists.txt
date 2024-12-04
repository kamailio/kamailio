cmake_minimum_required(VERSION 3.10)

# This is just for Debugging and print the compile definitions and options
# for the common target
get_target_property(
  MY_INTERFACE_DEFINITIONS common INTERFACE_COMPILE_DEFINITIONS
)
message(STATUS "Compile definitions for common: ${MY_INTERFACE_DEFINITIONS}")

get_target_property(MY_INTERFACE_OPTIONS common INTERFACE_COMPILE_OPTIONS)
message(STATUS "Compile options for common: ${MY_INTERFACE_OPTIONS}")

# -----------------------
# The following produces core/lex.yy.c and core/cfg.tab.c and core/cfg.tab.h
# files

# Find Flex
find_package(FLEX REQUIRED)

# Find Bison
find_package(BISON REQUIRED)

# Generate lex.yy.c from cfg.lex
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/src/core/lex.yy.c
  COMMAND ${FLEX_EXECUTABLE} -o ${CMAKE_CURRENT_SOURCE_DIR}/lex.yy.c
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg.lex
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cfg.lex
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.h
  COMMENT "Generating lex.yy.c from cfg.lex"
)

# Generate cfg.tab.c and cfg.tab.h from cfg.y
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.c
         ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.h
  COMMAND ${BISON_EXECUTABLE} -d -b cfg -o ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.c
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg.y
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cfg.y
  COMMENT "Generating cfg.tab.c and cfg.tab.h from cfg.y"
)

# Add library target to ensure the above commands run
add_library(
  GenerateParser ${CMAKE_CURRENT_SOURCE_DIR}/lex.yy.c
                 ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.c
)
# common_modules required so -fPIC is added to the compile options
target_link_libraries(GenerateParser PUBLIC common common_modules)

# -----------------------

file(GLOB_RECURSE kamailio_SRC "*.c")
set(kamailio_SRC
    ${kamailio_SRC}
    PARENT_SCOPE
)
# message(STATUS "kamailio_SRC: ${kamailio_SRC}")
