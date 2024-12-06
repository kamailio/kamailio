cmake_minimum_required(VERSION 3.10)

project(db_unixodbc)

find_package(ODBC REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(db_unixodbc SHARED ${SRC_FILES})
target_include_directories(db_unixodbc PRIVATE ${ODBC_INCLUDE_DIRS})
target_link_libraries(db_unixodbc PRIVATE kamailio ${ODBC_LIBRARIES})
