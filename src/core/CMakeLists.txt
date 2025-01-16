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

if(WIN32)
  cmake_policy(SET CMP0074 OLD)
endif()

# Generate lex.yy.c from cfg.lex
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
  COMMAND ${FLEX_EXECUTABLE} -o ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg.lex
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cfg.lex
          ${CMAKE_CURRENT_BINARY_DIR}/cfg.tab.h
  COMMENT "Generating lex.yy.c from cfg.lex"
)

# Generate cfg.tab.c and cfg.tab.h from cfg.y
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cfg.tab.c
         ${CMAKE_CURRENT_BINARY_DIR}/cfg.tab.h
  COMMAND ${BISON_EXECUTABLE} -d -b cfg -o ${CMAKE_CURRENT_BINARY_DIR}/cfg.tab.c
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg.y
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cfg.y
  COMMENT "Generating cfg.tab.c and cfg.tab.h from cfg.y"
)

# Add custom target for generating the 3 source files mentioned above
add_custom_target(
  GenerateParser ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
          ${CMAKE_CURRENT_BINARY_DIR}/cfg.tab.c
          ${CMAKE_CURRENT_BINARY_DIR}/cfg.tab.h
  COMMENT "Generating parser source files"
)

# -----------------------

file(GLOB_RECURSE kamailio_SRC "*.c")
list(REMOVE_ITEM kamailio_SRC ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lex.yy.c)
set(kamailio_SRC_FILES
    ${kamailio_SRC}
    PARENT_SCOPE
)
# target_include_directories(kamailio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# message(STATUS "kamailio_SRC: ${kamailio_SRC}")
