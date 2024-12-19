file(GLOB SRC_FILES "*.c")

# python2-dev package is required for Python2::Module target
find_package(Python2 REQUIRED COMPONENTS Development)

add_library(app_python SHARED ${SRC_FILES})
target_link_libraries(app_python PRIVATE Python2::Module)

target_compile_options(
  app_python
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/app_python/=")
set_target_properties(app_python PROPERTIES SUFFIX ".so")
