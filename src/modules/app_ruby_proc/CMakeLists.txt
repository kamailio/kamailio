file(GLOB SRC_FILES "*.c")

# ruby-dev package
find_package(Ruby REQUIRED)
add_library(app_ruby_proc SHARED ${SRC_FILES})

target_include_directories(app_ruby_proc PRIVATE ${Ruby_INCLUDE_DIRS})
target_link_libraries(app_ruby_proc PRIVATE ${Ruby_LIBRARIES})

target_compile_options(app_ruby_proc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/app_ruby_proc/=")
