file(GLOB SRC_FILES "*.c")

option(LUAJIT "Enable LuaJIT (for app_lua modules)" OFF)

if(LUAJIT)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LUA REQUIRED IMPORTED_TARGET luajit)
else()
  find_package(Lua REQUIRED)
  # CMake FindLua defines include as LUA_INCLUDE_DIR and lib as LUA_LIBRARIES
  set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
endif()

add_library(app_lua SHARED ${SRC_FILES})
# the LUA_COMPAT_MODULE fixes this error: implicit declaration of function
# ‘luaL_openlib
target_compile_definitions(app_lua PRIVATE LUA_COMPAT_MODULE)
target_include_directories(app_lua PRIVATE ${LUA_INCLUDE_DIRS})
target_link_libraries(app_lua PRIVATE ${LUA_LIBRARIES})
