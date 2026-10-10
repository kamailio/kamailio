if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "${module_name}: module is supported only on Linux (epoll, timerfd)")
endif()

file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(CURL REQUIRED)

target_link_libraries(${module_name} PRIVATE CURL::libcurl)
