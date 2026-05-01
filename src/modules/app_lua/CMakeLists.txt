file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

option(LUAJIT "Enable LuaJIT (for app_lua modules)" OFF)

set(_ksr_lua_version_default "")
if(DEFINED ENV{KSR_LUA_VERSION} AND NOT "$ENV{KSR_LUA_VERSION}" STREQUAL "")
  set(_ksr_lua_version_default "$ENV{KSR_LUA_VERSION}")
endif()
set(KSR_LUA_VERSION
    "${_ksr_lua_version_default}"
    CACHE STRING "Lua version to use for app_lua (for example: 5.1, 5.2, 5.3, 5.4)"
)

if(LUAJIT)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LUA REQUIRED IMPORTED_TARGET luajit)
  set(LUA_INCLUDE_DIR ${LUA_INCLUDE_DIRS})
else()
  if(KSR_LUA_VERSION)
    find_package(Lua ${KSR_LUA_VERSION} QUIET)
    if(Lua_FOUND)
      message(STATUS "app_lua using Lua ${KSR_LUA_VERSION} via find_package(Lua)")
    else()
      find_package(PkgConfig REQUIRED)
      string(REPLACE "." "" _ksr_lua_version_nodot "${KSR_LUA_VERSION}")
      set(_ksr_lua_pkg_found FALSE)
      foreach(_ksr_lua_pkg lua${KSR_LUA_VERSION} lua-${KSR_LUA_VERSION}
                           lua${_ksr_lua_version_nodot}
      )
        if(NOT _ksr_lua_pkg_found)
          pkg_check_modules(LUA QUIET ${_ksr_lua_pkg})
          if(LUA_FOUND)
            set(_ksr_lua_pkg_found TRUE)
            message(
              STATUS "app_lua using pkg-config module '${_ksr_lua_pkg}' for Lua ${KSR_LUA_VERSION}"
            )
          endif()
        endif()
      endforeach()
      if(NOT _ksr_lua_pkg_found)
        message(
          FATAL_ERROR
            "Could not find Lua version '${KSR_LUA_VERSION}' for app_lua. "
            "Set KSR_LUA_VERSION (or environment KSR_LUA_VERSION/LUA_VERSION) "
            "to an installed version such as 5.1, 5.2, 5.3 or 5.4."
        )
      endif()
      set(LUA_INCLUDE_DIR ${LUA_INCLUDE_DIRS})
    endif()
  else()
    find_package(Lua)
  endif()
  if(NOT Lua_FOUND AND NOT LUA_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua)
    set(LUA_INCLUDE_DIR ${LUA_INCLUDE_DIRS})
  endif()
endif()

# the LUA_COMPAT_MODULE fixes this error: - implicit declaration of function
# luaL_openlib
target_compile_definitions(${module_name} PRIVATE LUA_COMPAT_MODULE)
target_include_directories(${module_name} PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(${module_name} PRIVATE ${LUA_LIBRARIES})
