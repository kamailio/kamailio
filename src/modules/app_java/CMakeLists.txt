file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# TODO: GCJ is a discontiuned project from 2018 sudo apt install openjdk-13-jdk
# compiles with warnings
find_package(JNI REQUIRED)

target_compile_options(${module_name} PRIVATE -O0 -g)
target_include_directories(${module_name} PRIVATE JNI_INCLUDE_DIRS)
target_link_libraries(${module_name} PRIVATE JNI::JNI)
