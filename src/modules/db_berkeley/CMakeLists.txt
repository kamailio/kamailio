# libdb++-dev
find_package(BerkeleyDB REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(db_berkeley SHARED ${SRC_FILES})
target_link_libraries(db_berkeley PRIVATE BerkeleyDB::BerkeleyDB)

target_compile_options(
  db_berkeley
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/db_berkeley/=")
set_target_properties(db_berkeley PROPERTIES SUFFIX ".so")
