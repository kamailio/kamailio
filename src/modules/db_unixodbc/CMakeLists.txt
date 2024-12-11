find_package(ODBC REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(db_unixodbc SHARED ${SRC_FILES})
target_include_directories(db_unixodbc PRIVATE ${ODBC_INCLUDE_DIRS})
target_link_libraries(db_unixodbc PRIVATE ${ODBC_LIBRARIES})

target_compile_options(db_unixodbc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/db_unixodbc/=")
