# Modules
set(MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Initialize a global property to track added modules
set_property(GLOBAL PROPERTY ADDED_MODULES_LIST "")
set_property(GLOBAL PROPERTY ALL_MODULE_DOC_TARGETS "")
set_property(GLOBAL PROPERTY EXCLUDED_MODULES_LIST "")

include(${CMAKE_SOURCE_DIR}/cmake/groups.cmake)

# AVAIABLE_GROUPS is defined in groups.cmake
set(AVAILABLE_GROUP_NAMES ${AVAILABLE_GROUPS})
set(MODULE_GROUP_NAME
    "DEFAULT"
    CACHE STRING "Group of modules to build (one or multiple space seperated group)"
)
set_property(CACHE MODULE_GROUP_NAME PROPERTY STRINGS ${AVAILABLE_GROUP_NAMES})

# User may provide multiple groups seperated by space
separate_arguments(MODULE_GROUP_LIST UNIX_COMMAND "${MODULE_GROUP_NAME}")
message(STATUS "Building groups: ${MODULE_GROUP_LIST}")

# Check if elements in MODULE_GROUP_LIST are valid by checking against
# AVAILABLE_GROUP_NAMES
set(FULL_MODULE_GROUP_NAMES "")
foreach(group_name IN LISTS MODULE_GROUP_LIST)
  list(FIND AVAILABLE_GROUP_NAMES "${group_name}" INDEX)
  if(INDEX EQUAL -1)
    message(FATAL_ERROR "Invalid module group specified: ${group_name}.
        Available groups: ${AVAILABLE_GROUPS}"
    )
  else()
    list(APPEND FULL_MODULE_GROUP_NAMES "MODULE_GROUP_${group_name}")
    # get_property(MODULE_GROUP VARIABLE PROPERTY "MODULE_GROUP_${GROUP_NAME}")
    # message(STATUS "Building module group: MODULE_GROUP_${GROUP_NAME}")
    # message(STATUS "Modules: ${MODULE_GROUP}")
  endif()
endforeach()
# message(STATUS "Expanded module groups: ${FULL_MODULE_GROUP_NAMES}")

# Allow users to specify extra modules to build
set(INCLUDE_MODULES
    ""
    CACHE STRING "List of extra modules to build (space-separated)"
)

set(EXCLUDE_MODULES
    ""
    CACHE STRING "List of modules to exclude from building (space-separated)"
)

include(${CMAKE_SOURCE_DIR}/cmake/modules-docs.cmake)

# Function to add modules from a list
function(add_module_group group_name group_modules)
  # message(STATUS "Adding modules for group: ${group_name}")
  # message(STATUS "Modules: ${group_modules}")

  foreach(module_name IN LISTS group_modules)
    # Check if the module has already been added
    get_property(ALREADY_ADDED GLOBAL PROPERTY ADDED_MODULES_LIST)
    if("${module_name}" IN_LIST ALREADY_ADDED)
      message(STATUS "Module ${module_name} has already been added. Skipping...")
    elseif("${module_name}" IN_LIST EXCLUDED_MODULES_LIST)
      message(STATUS "Module ${module_name} is excluded. Skipping...")
      list(REMOVE_ITEM ADDED_MODULES_LIST ${module_name})
    else()
      # Construct the path to the module
      set(module_path "${MODULES_DIR}/${module_name}")
      # message(STATUS "Adding module: ${MODULE_NAME} (${MODULE_PATH})")

      # Check if the directory exists before adding
      if(IS_DIRECTORY ${module_path} AND EXISTS ${module_path}/CMakeLists.txt)
        add_subdirectory(${module_path})
        # Remove the 'lib' prefix from the module name
        set_target_properties(${module_name} PROPERTIES PREFIX "")
        # Set suffix to .so always
        set_target_properties(${module_name} PROPERTIES SUFFIX ".so")

        # Add this policy to new so we can use target_link_libraries(... PRIVATE
        # ...) to link against targets defined in children directories
        if(POLICY CMP0079)
          cmake_policy(SET CMP0079 NEW)
        endif()

        target_link_libraries(${module_name} PRIVATE common_modules)
        target_compile_definitions(
          ${module_name} PRIVATE MOD_NAMEID=${module_name} MOD_NAME=\"${module_name}\"
        )

        target_compile_options(
          ${module_name}
          PRIVATE "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/${module_name}/="
        )

        # Set the RPATH of the module to include the installation directory of
        set_target_properties(
          ${module_name} PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}"
        )

        # Install the module to the appropriate directory under the installation
        # prefix
        install(
          TARGETS ${module_name}
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/modules
          COMPONENT ${group_name}
        )
        # Add the module to the list of added modules
        set_property(GLOBAL APPEND PROPERTY ADDED_MODULES_LIST ${module_name})
        docs_add_module(${group_name} ${module_name})
        set_property(GLOBAL APPEND PROPERTY ALL_MODULE_DOC_TARGETS ${module_name}_doc)
      else()
        message(FATAL_ERROR "Module directory ${module_path} does not exist.")
      endif()
    endif()
  endforeach()
endfunction()

# Define targets before adding the groups and modules, so that we can add
# dependencies to them.
add_custom_target(
  kamailio_docs_readme
  COMMENT "Generating modules readme. Note: This will overwrite REAMDEs found in source tree"
)

add_custom_target(kamailio_docs_man COMMENT "Generating man pages")

# Parse and add extra modules specified by the user
separate_arguments(INCLUDE_MODULES_LIST UNIX_COMMAND "${INCLUDE_MODULES}")
message(STATUS "Extra modules (if module already included already from group is ignored):
    ${INCLUDE_MODULES_LIST}"
)
list(APPEND USER_MODULES_LIST ${INCLUDE_MODULES_LIST})

separate_arguments(EXCLUDED_MODULES_LIST UNIX_COMMAND "${EXCLUDE_MODULES}")
message(STATUS "Excluded modules (exlcude even if in included modules):
    ${EXCLUDED_MODULES_LIST}"
)

# Add each group of modules
foreach(group IN LISTS FULL_MODULE_GROUP_NAMES)
  get_property(MODULES_IN_GROUP VARIABLE PROPERTY "${group}")
  if(VERBOSE)
    message(STATUS "Modules in ${group}: ${MODULES_IN_GROUP}")
  endif()
  # remove module_group_prefix.
  # TODO: This feels like a hack since user already provided
  # the group name. Move the check above in these for maybe?
  string(REPLACE "MODULE_GROUP_" "" group_name "${group}")
  # message(WARNING "Adding module group: ${group_name}")
  add_module_group("${group_name}" "${MODULES_IN_GROUP}")
endforeach()
add_module_group(" " "${USER_MODULES_LIST}")

get_property(ADDED_MODULES_LIST GLOBAL PROPERTY ADDED_MODULES_LIST)
message(STATUS "Modules to be built: ${ADDED_MODULES_LIST}")
# TODO: Comment is not showing. probably a bug in CMAKE
# https://gitlab.kitware.com/cmake/cmake/-/issues/26571
add_custom_target(modules COMMENT "Building modules")
if(NOT ADDED_MODULES_LIST)
  message(WARNING "No modules to build. Did you forget to add a module group or include module?")
else()
  add_dependencies(modules ${ADDED_MODULES_LIST})
endif()
get_property(ALL_MODULE_DOC_TARGETS GLOBAL PROPERTY ALL_MODULE_DOC_TARGETS)
# message(STATUS "ALL_MODULE_DOC_TARGETS: ${ALL_MODULE_DOC_TARGETS}")

# Add a kamailio_docs target that depends on all module documentation targets
add_custom_target(
  kamailio_docs
  ${docs_in_all_target}
  DEPENDS ${ALL_MODULE_DOC_TARGETS}
  COMMENT "Generating Kamailio documentation"
)

# No component kamailio_docs is defined after component based installation
# TODO: Fix if possible
add_custom_target(
  install_kamailio_docs
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component kamailio_docs
  DEPENDS kamailio_docs
  COMMENT "Installing Kamailio documentation"
)
