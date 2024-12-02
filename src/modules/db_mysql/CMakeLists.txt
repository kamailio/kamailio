cmake_minimum_required(VERSION 3.10)

project(db_mysql)

find_package(MySQL QUIET)
if (NOT MySQL_FOUND)
    message(STATUS "MySQL not found with CMake FindMySQL.cmake. Trying with pkg-config")
    find_package(PkgConfig QUIET)
    pkg_check_modules(MySQL mysqlclient>=5.7)
    if (NOT MySQL_FOUND)
        message(AUTHOR_WARNING "MySQL not found with pkg-config. Trying with MariaDB")
    endif()
endif()

if (NOT MySQL_FOUND)
    find_package(MariaDB QUIET)
    if(NOT MariaDB_FOUND)
        message(STATUS "MariaDB not found. Trying with pkg-config")
        find_package(PkgConfig QUIET)
        pkg_check_modules(MariaDB mariadb)
        if(NOT MariaDB_FOUND)
            message(AUTHOR_WARNING "MariaDB not found with pkg-config")
        endif()
        message(FATAL_ERROR "Neither MySQL nor MariaDB found. Please install MySQL or MariaDB dev libraries")
    else()
        message(STATUS "MariaDB found")
    endif()
endif()

file(GLOB mySQL_SRC "*.c")

add_library(db_mysql SHARED ${mySQL_SRC})
target_include_directories(db_mysql PRIVATE ${MySQL_INCLUDE_DIRS})
target_link_libraries(db_mysql PRIVATE ${MySQL_LIBRARIES} common)

# TODO Install script files for kamctl