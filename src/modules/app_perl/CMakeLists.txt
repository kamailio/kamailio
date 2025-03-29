file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# libperl-dev
find_package(PerlLibs REQUIRED)

target_include_directories(${module_name} PRIVATE ${PERL_INCLUDE_PATH})
# message(STATUS "PERL_EXTRA_C_FLAGS: ${PERL_EXTRA_C_FLAGS}")

# https://cmake.org/cmake/help/latest/command/target_compile_options.html
# Use SHELL: to avoid quoting issues and seperate multiple flags with spaces
target_compile_options(${module_name} PRIVATE SHELL:${PERL_EXTRA_C_FLAGS})
target_link_libraries(${module_name} PRIVATE ${PERL_LIBRARY})

# Give the user option to override the default ldopts and typemap
set(PERL_LDOPTS
    ""
    CACHE STRING "Extra perl linker options"
)

set(PERL_CCOPTS
    ""
    CACHE STRING "Extra perl compiler options"
)

set(PERL_LIBPATH
    ""
    CACHE STRING "Path to perl library"
)

set(PERL_TYPEMAP
    ""
    CACHE FILEPATH "Path to typemap file"
)

if(NOT PERL_LDOPTS)
  execute_process(
    COMMAND ${PERL_EXECUTABLE} -MExtUtils::Embed -e ldopts
    RESULT_VARIABLE PERL_LDOPTS_RESULT # 0 if success,
    OUTPUT_VARIABLE PERL_LDOPTS # output of the command
    ERROR_VARIABLE PERL_LDOPTS_ERROR # error of the command if any
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE
  )
  string(STRIP ${PERL_LDOPTS} PERL_LDOPTS)
  message(STATUS "PERL_LDOPTS: ${PERL_LDOPTS}")
endif()
target_link_options(${module_name} PRIVATE SHELL:${PERL_LDOPTS})

if(NOT PERL_CCOPTS)
  execute_process(
    COMMAND ${PERL_EXECUTABLE} -MExtUtils::Embed -e ccopts
    RESULT_VARIABLE PERL_CCOPTS_RESULT # 0 if success,
    OUTPUT_VARIABLE PERL_CCOPTS # output of the command
    ERROR_VARIABLE PERL_CCOPTS_ERROR # error of the command if any
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE
  )
  string(STRIP ${PERL_CCOPTS} PERL_CCOPTS)
  message(STATUS "PERL_CCOPTS: [${PERL_CCOPTS}]")
endif()
target_compile_options(${module_name} PRIVATE SHELL:${PERL_CCOPTS})

if(NOT PERL_TYPEMAP)
  if(NOT PERL_LIBPATH)
    execute_process(
      COMMAND ${PERL_EXECUTABLE} -MConfig -e "print \$Config{installprivlib}"
      RESULT_VARIABLE PERL_LIBPATH_RESULT # 0 if success,
      OUTPUT_VARIABLE PERL_LIBPATH # output of the command
      ERROR_VARIABLE PERL_LIBPATH_ERROR # error of the command if any
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE
    )
    message(WARNING "PERL_LIBPATH: ${PERL_LIBPATH}")
  endif()
  set(PERL_TYPEMAP ${PERL_LIBPATH}/ExtUtils/typemap)
  message(WARNING "PERL_TYPEMAP: ${PERL_TYPEMAP}")
endif()

set(PERL_PODFILES ${CMAKE_CURRENT_SOURCE_DIR}/kamailioxs.xs)
file(GLOB_RECURSE PERL_PODFILES_PM "${CMAKE_CURRENT_SOURCE_DIR}/lib/perl/*.pm")
list(APPEND PERL_PODFILES ${PERL_PODFILES_PM})

# Generate kamalioxs.c file
# This is generated in source directory, because the paths in .c file a relative to the current source directory
# and we don't want to change them.
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/kamailioxs.c
  COMMAND
    xsubpp -typemap ${PERL_TYPEMAP} -typemap ${CMAKE_CURRENT_SOURCE_DIR}/typemap
    kamailioxs.xs > ${CMAKE_CURRENT_SOURCE_DIR}/kamailioxs.c
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating kamailioxs.c"
  VERBATIM
)

# Add the generated file to the module sources
target_sources(${module_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kamailioxs.c)
# Add the flag option only for the generated file
set_source_files_properties(
  ${CMAKE_CURRENT_SOURCE_DIR}/kamailioxs.c PROPERTIES COMPILE_OPTIONS
                                                      -Wno-unused
)

# Doc target
# OLD MAKEFILE perlpod seems to do nothing
add_custom_target(perlpod)

# app_perl.sgml
# Requires libpod-2-docbook-perl packages
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/app_perl_pod.sgml
  COMMAND cat ${PERL_PODFILES} | pod2docbook --doctype=chapter --title='Kamailio
          Perl API' --no-header - ${CMAKE_CURRENT_BINARY_DIR}/app_perl_pod.sgml
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating app_perl_pod.sgml"
)

add_custom_target(
  ${module_name}_pod.sgml
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/app_perl_pod.sgml
)

# Install nessecary files
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/perl
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}
)
