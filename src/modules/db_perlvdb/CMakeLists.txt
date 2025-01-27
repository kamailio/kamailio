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
