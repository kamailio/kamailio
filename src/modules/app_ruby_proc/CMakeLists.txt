cmake_minimum_required(VERSION 3.10)

project(app_ruby_proc)

file(GLOB SRC_FILES "*.c")

# ruby-dev package
find_package(Ruby REQUIRED)
add_library(app_ruby_proc SHARED ${SRC_FILES})

target_include_directories(app_ruby_proc PRIVATE ${Ruby_INCLUDE_DIRS})
target_link_libraries(app_ruby_proc PRIVATE kamailio ${Ruby_LIBRARIES})
