file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

option(LUAJIT "Enable LuaJIT (for app_lua modules)" OFF)

if(LUAJIT)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LUA REQUIRED IMPORTED_TARGET luajit)
else()
  find_package(Lua REQUIRED)
  # CMake FindLua defines include as LUA_INCLUDE_DIR and lib as LUA_LIBRARIES
  set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
endif()

# the LUA_COMPAT_MODULE fixes this error: - implicit declaration of function
# luaL_openlib
target_compile_definitions(${module_name} PRIVATE LUA_COMPAT_MODULE)
target_include_directories(${module_name} PRIVATE ${LUA_INCLUDE_DIRS})
target_link_libraries(${module_name} PRIVATE ${LUA_LIBRARIES})
