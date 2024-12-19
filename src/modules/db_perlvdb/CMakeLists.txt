find_package(PerlLibs REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(db_perlvdb SHARED ${SRC_FILES})
target_include_directories(db_perlvdb PRIVATE ${PERL_INCLUDE_PATH})
target_compile_definitions(db_perlvdb PRIVATE ${PERL_EXTRA_C_FLAGS})
target_link_libraries(db_perlvdb PRIVATE ${PERL_LIBRARY})

target_compile_options(
  db_perlvdb
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/db_perlvdb/=")
set_target_properties(db_perlvdb PROPERTIES SUFFIX ".so")
