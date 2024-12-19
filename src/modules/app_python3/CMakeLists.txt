file(GLOB SRC_FILES "*.c")

find_package(Python3 REQUIRED COMPONENTS Development)

add_library(app_python3 SHARED ${SRC_FILES})
target_link_libraries(app_python3 PRIVATE Python3::Python)

target_compile_options(
  app_python3
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/app_python3/=")
set_target_properties(app_python3 PROPERTIES SUFFIX ".so")
