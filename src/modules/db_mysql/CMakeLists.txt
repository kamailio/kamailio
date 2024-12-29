file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(MariaDBClient REQUIRED)

if(NOT MariaDBClient_FOUND)
  message(FATAL_ERROR "MySQL or MariaDBClient not found")
  message(FATAL_ERROR "Please install the dev libraries")
endif()

target_include_directories(${module_name} PRIVATE ${MariaDBClient_INCLUDE_DIRS})
target_link_libraries(${module_name} PRIVATE ${MariaDBClient_LIBRARIES})

# TODO Install script files for kamctl
