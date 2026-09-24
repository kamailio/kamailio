file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
add_library(uuid::uuid ALIAS PkgConfig::UUID)

target_link_libraries(${module_name} PRIVATE uuid::uuid)

# uuid_generate_time_v7() is available since util-linux 2.41; without it
# $uuid(7) is not supported at runtime (returns $null)
include(CheckSymbolExists)
include(CMakePushCheckState)
cmake_push_check_state()
set(CMAKE_REQUIRED_INCLUDES ${UUID_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${UUID_LINK_LIBRARIES})
check_symbol_exists(uuid_generate_time_v7 "uuid/uuid.h" HAVE_UUID_GENERATE_TIME_V7)
cmake_pop_check_state()
if(HAVE_UUID_GENERATE_TIME_V7)
  target_compile_definitions(${module_name} PRIVATE HAVE_UUID_GENERATE_TIME_V7)
endif()
