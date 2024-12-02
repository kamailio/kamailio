cmake_minimum_required(VERSION 3.10)

get_target_property(MY_INTERFACE_DEFINITIONS common INTERFACE_COMPILE_DEFINITIONS)
message(STATUS "Compile definitions for common: ${MY_INTERFACE_DEFINITIONS}")

get_target_property(MY_INTERFACE_OPTIONS common INTERFACE_COMPILE_OPTIONS)
message(STATUS "Compile options for common: ${MY_INTERFACE_OPTIONS}")


# -----------------------
# The following produces core/lex.yy.c and core/cfg.tab.c and core/cfg.tab.h files

# Find Flex
find_package(FLEX REQUIRED)

# Find Bison
find_package(BISON REQUIRED)

# Generate lex.yy.c from cfg.lex
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/lex.yy.c
  COMMAND ${FLEX_EXECUTABLE} -o ${CMAKE_CURRENT_SOURCE_DIR}/lex.yy.c ${CMAKE_CURRENT_SOURCE_DIR}/cfg.lex
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cfg.lex ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.h
  COMMENT "Generating lex.yy.c from cfg.lex"
)

# Generate cfg.tab.c and cfg.tab.h from cfg.y
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.c ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.h
  COMMAND ${BISON_EXECUTABLE} -d -b cfg -o ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.c ${CMAKE_CURRENT_SOURCE_DIR}/cfg.y
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cfg.y
  COMMENT "Generating cfg.tab.c and cfg.tab.h from cfg.y"
)

# Custom target to ensure the above commands run
add_custom_target(GenerateParser ALL
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lex.yy.c ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.c ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.h
)

# -----------------------

file(GLOB_RECURSE kamailio_SRC
"${CMAKE_SOURCE_DIR}/main.c"
"*.c"
)

# message(STATUS "kamailio_SRC: ${kamailio_SRC}" )
add_executable(kamailio ${kamailio_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/lex.yy.c ${CMAKE_CURRENT_SOURCE_DIR}/cfg.tab.c)
target_link_libraries(kamailio PUBLIC common m
                                Threads::Threads # Pthread library
                                dl      # dlopen and more
                                resolv  # resolv
)
# Make sure the cfg.tab.c and lex.yy.c are generated before building kamailio
add_dependencies(kamailio GenerateParser)
# Enable ENABLE_EXPORTS property so modules can link from symbols found in main.c and core/*.
set_target_properties(kamailio PROPERTIES ENABLE_EXPORTS TRUE)
# Add the MODS_DIR definition required by main.c
target_compile_definitions(kamailio PUBLIC
    MODS_DIR="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/modules"
)