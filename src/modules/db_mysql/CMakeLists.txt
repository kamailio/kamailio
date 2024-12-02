cmake_minimum_required(VERSION 3.10)

project(db_mysql)

find_package(MariaDBClient REQUIRED)

if(NOT MariaDBClient_FOUND)
  message(FATAL_ERROR "MySQL or MariaDBClient not found")
  message(FATAL_ERROR "Please install the dev libraries")
endif()

file(GLOB mySQL_SRC "*.c")

add_library(db_mysql SHARED ${mySQL_SRC})
target_include_directories(db_mysql PRIVATE ${MariaDBClient_INCLUDE_DIRS})
target_link_libraries(db_mysql PRIVATE ${MariaDBClient_LIBRARIES} common)

# TODO Install script files for kamctl
