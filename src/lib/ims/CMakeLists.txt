cmake_minimum_required(VERSION 3.10)

project(kamailio_lib_ims VERSION 0.1.0)

find_package(LibXml2 REQUIRED)
file(GLOB SRC_FILES "*.c")

# Original target to be named as folder, so when included from lib/CMakeLists.txt
# it will be named as lib/ims.so
add_library(lib_ims SHARED ${SRC_FILES})
target_link_libraries(lib_ims common kamailio)

# TODO: do we need version control?
set_target_properties(
  lib_ims PROPERTIES VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
                     OUTPUT_NAME ims
)
set_target_properties(lib_ims PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})

# Target alias for linking
# TODO: Check how is handled in makefiles
add_library(kamailio_ims ALIAS lib_ims)
