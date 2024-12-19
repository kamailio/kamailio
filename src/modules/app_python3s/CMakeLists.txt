file(GLOB SRC_FILES "*.c")

find_package(Python3 REQUIRED COMPONENTS Development)

add_library(app_python3s STATIC ${SRC_FILES})
target_link_libraries(app_python3s PRIVATE Python3::Module)

target_compile_options(
  app_python3s
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/app_python3s/=")
set_target_properties(app_python3s PROPERTIES SUFFIX ".so")
