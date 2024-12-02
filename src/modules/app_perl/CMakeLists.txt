cmake_minimum_required(VERSION 3.10)

project(app_perl)

file(GLOB SRC_FILES "*.c")

# libperl-dev
find_package(PerlLibs REQUIRED)

add_library(app_perl SHARED ${SRC_FILES})
target_include_directories(app_perl PRIVATE ${PERL_INCLUDE_PATH})
# message(STATUS "PERL_EXTRA_C_FLAGS: ${PERL_EXTRA_C_FLAGS}")
target_compile_definitions(app_perl PRIVATE ${PERL_EXTRA_C_FLAGS})
target_link_libraries(app_perl PRIVATE kamailio ${PERL_LIBRARY})
