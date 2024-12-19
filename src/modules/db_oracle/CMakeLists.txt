# TODO: Needs some testing for OS that support OracleDB
find_package(Oracle REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(db_oracle SHARED ${SRC_FILES})
target_include_directories(db_oracle PRIVATE ${Oracle_INCLUDE_DIRS}
)# /usr/include/oracle/18.3/client64)
target_link_libraries(db_oracle PRIVATE occi clntsh)

# target_link_libraries(.*PRIVATE)

target_compile_options(
  db_oracle
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/db_oracle/=")
set_target_properties(db_oracle PROPERTIES SUFFIX ".so")
