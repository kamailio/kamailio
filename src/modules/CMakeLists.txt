# Modules
set(MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Initialize a global property to track added modules
set_property(GLOBAL PROPERTY ADDED_MODULES_LIST "")
set_property(GLOBAL PROPERTY ALL_MODULE_DOC_TARGETS "")

include(groups.cmake)

# TODO: Add proper search for all or find better way
# TODO: Allow the selection of a group of modules defined in groups.cmake
set(MODULE_GROUP_NAMES ${AVAILABLE_GROUPS})
set(MODULE_GROUP_NAME
    "DEFAULT"
    CACHE STRING "Group of modules to build (all, or a custom group)"
)

# Check if the specified group exists
list(FIND AVAILABLE_GROUPS "${MODULE_GROUP_NAME}" GROUP_INDEX)
if(GROUP_INDEX EQUAL -1)
  message(
    FATAL_ERROR
      "Invalid module group specified: ${MODULE_GROUP_NAME}. Available groups: ${AVAILABLE_GROUPS}"
  )
endif()

if(MODULE_GROUP_NAME STREQUAL "ALL")
  list(SORT MODULE_GROUP_ALL)
  set(MODULE_GROUP ${MODULE_GROUP_ALL})
elseif(MODULE_GROUP_NAME STREQUAL "DEFAULT")
  list(SORT MODULE_GROUP_DEFAULT)
  set(MODULE_GROUP ${MODULE_GROUP_DEFAULT})
endif()

# list(SORT MODULE_GROUP_DEFAULT)
# message(STATUS "MODULE_GROUP_DEFAULT: ${MODULE_GROUP_DEFAULT}")

# Allow users to specify extra modules to build
set(INCLUDE_MODULES
    ""
    CACHE STRING "List of extra modules to build (space-separated)"
)

set(EXCLUDE_MODULES
    ""
    CACHE STRING "List of modules to exclude from building (space-separated)"
)

include(${CMAKE_SOURCE_DIR}/cmake/modules_docs.cmake)

# Function to add modules from a list
function(add_module_group GROUP_MODULES)
  foreach(MODULE_NAME IN LISTS GROUP_MODULES)
    # Check if the module has already been added
    get_property(ALREADY_ADDED GLOBAL PROPERTY ADDED_MODULES_LIST)
    if(";${ALREADY_ADDED};" MATCHES ";${MODULE_NAME};")
      message(
        STATUS "Module ${MODULE_NAME} has already been added. Skipping..."
      )
    else()
      # Construct the path to the module
      set(MODULE_PATH "${MODULES_DIR}/${MODULE_NAME}")
      # message(STATUS "Adding module: ${MODULE_NAME} (${MODULE_PATH})")
      # Check if the directory exists before adding
      if(IS_DIRECTORY ${MODULE_PATH} AND EXISTS ${MODULE_PATH}/CMakeLists.txt)
        add_subdirectory(${MODULE_PATH})
        # Remove the 'lib' prefix from the module name
        set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

        # Add this policy to new so we can use target_link_libraries(... PRIVATE ...)
        # to link against targets defined in children directories
        if(POLICY CMP0079)
          cmake_policy(SET CMP0079 NEW)
        endif()
        target_link_libraries(${MODULE_NAME} PRIVATE common_modules)
        target_compile_definitions(
          ${MODULE_NAME} PRIVATE MOD_NAMEID=${MODULE_NAME}
                                 MOD_NAME=\"${MODULE_NAME}\"
        )
        set_target_properties(
          ${MODULE_NAME}
          PROPERTIES
            INSTALL_RPATH
            "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
        )
        # Install the module to the appropriate directory under the installation
        # prefix
        install(
          TARGETS ${MODULE_NAME}
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/modules
          COMPONENT kamailio_modules
        )
        # Add the module to the list of added modules
        set_property(GLOBAL APPEND PROPERTY ADDED_MODULES_LIST ${MODULE_NAME})
        docs_add_module(${MODULE_NAME})
        set_property(
          GLOBAL APPEND PROPERTY ALL_MODULE_DOC_TARGETS ${MODULE_NAME}_doc
        )
      else()
        message(WARNING "Module directory ${MODULE_PATH} does not exist.")
      endif()
    endif()
  endforeach()
endfunction()

list(APPEND FINAL_MODULES_LIST ${MODULE_GROUP})

# Parse and add extra modules specified by the user
separate_arguments(INCLUDE_MODULES_LIST UNIX_COMMAND "${INCLUDE_MODULES}")
message(STATUS "Extra modules: ${INCLUDE_MODULES_LIST}")
list(APPEND FINAL_MODULES_LIST ${INCLUDE_MODULES_LIST})

# Exclude modules that were explicitly excluded by the user
separate_arguments(EXCLUDE_MODULES_LIST UNIX_COMMAND "${EXCLUDE_MODULES}")
message(STATUS "Excluded modules: ${EXCLUDE_MODULES_LIST}")
foreach(MODULE_NAME IN LISTS EXCLUDE_MODULES_LIST)
  # if in list remove it and print a message
  if(";${FINAL_MODULES_LIST};" MATCHES ";${MODULE_NAME};")
    list(REMOVE_ITEM FINAL_MODULES_LIST ${MODULE_NAME})
    message(STATUS "Removed module: ${MODULE_NAME}")
  endif()
endforeach()

# Add each group of modules
add_module_group("${FINAL_MODULES_LIST}")

# Add a kamailio_docs target that depends on all module documentation targets
get_property(ALL_MODULE_DOC_TARGETS GLOBAL PROPERTY ALL_MODULE_DOC_TARGETS)
# message(STATUS "ALL_MODULE_DOC_TARGETS: ${ALL_MODULE_DOC_TARGETS}")

add_custom_target(
  kamailio_docs
  DEPENDS ${ALL_MODULE_DOC_TARGETS}
  COMMENT "Generating Kamailio documentation"
)
# add_dependencies(kamailio-docs kamailio_docs)

add_custom_target(
  install_kamailio_docs
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component
          kamailio_docs
  DEPENDS kamailio_docs
)
