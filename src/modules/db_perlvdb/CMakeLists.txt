cmake_minimum_required(VERSION 3.10)

project(db_perlvdb)

find_package(PerlLibs REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(db_perlvdb SHARED ${SRC_FILES})
target_include_directories(db_perlvdb PRIVATE ${PERL_INCLUDE_PATH})
target_compile_definitions(db_perlvdb PRIVATE ${PERL_EXTRA_C_FLAGS})
target_link_libraries(db_perlvdb PRIVATE kamailio ${PERL_LIBRARY})
target_link_libraries(db_perlvdb PRIVATE srdb1)
