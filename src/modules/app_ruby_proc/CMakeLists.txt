file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# ruby-dev package
find_package(Ruby REQUIRED)

target_include_directories(${module_name} PRIVATE ${Ruby_INCLUDE_DIRS})
target_link_libraries(${module_name} PRIVATE ${Ruby_LIBRARIES})
