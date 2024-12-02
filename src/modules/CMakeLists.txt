# Modules
set(MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Initialize a global property to track added modules
set_property(GLOBAL PROPERTY ADDED_MODULES_LIST "")
set_property(GLOBAL PROPERTY ALL_MODULE_DOC_TARGETS "")

include(groups.cmake)

# TODO: Add proper search for all or find better way
# TODO: Allow the selection of a group of modules defined in groups.cmake
set(MODULE_GROUP_NAME
    ""
    CACHE STRING "Group of modules to build (all, or a custom group)"
)
# If no group is specified, use the default group and print the available groups
if(NOT MODULE_GROUP_NAME)
  message(STATUS "Available groups: ${AVAILABLE_GROUPS}")
  message(STATUS "No module group specified. Using default group.")
  set(MODULE_GROUP_NAME "MODULE_GROUP_DEFAULT")
  set(MODULE_GROUP ${MODULE_GROUP_DEFAULT})
endif()

# Check if the specified group exists
list(FIND AVAILABLE_GROUPS "${MODULE_GROUP_NAME}" GROUP_INDEX)
if(GROUP_INDEX EQUAL -1)
  message(
    FATAL_ERROR "Invalid module group specified: ${MODULE_GROUP_NAME}. Available groups: ${AVAILABLE_GROUPS}"
  )
endif()

if (MODULE_GROUP_NAME STREQUAL "MODULE_GROUP_ALL")
  set(MODULE_GROUP ${MODULE_GROUP_ALL})
endif()
message(STATUS "Building modules in ${MODULE_GROUP_NAME}: ${MODULE_GROUP}")

# list(SORT MODULE_GROUP_DEFAULT)
# message(STATUS "MODULE_GROUP_DEFAULT: ${MODULE_GROUP_DEFAULT}")

# Allow users to specify extra modules to build
set(INCLUDE_MODULES
    ""
    CACHE STRING "List of extra modules to build (space-separated)"
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
      # Check if the directory exists before adding
      if(IS_DIRECTORY ${MODULE_PATH} AND EXISTS ${MODULE_PATH}/CMakeLists.txt)
        add_subdirectory(${MODULE_PATH})
        # Remove the 'lib' prefix from the module name
        set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
        target_compile_definitions(
          ${MODULE_NAME} PRIVATE MOD_NAMEID=${MODULE_NAME}
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

# Add each group of modules
add_module_group(${MODULE_GROUP})

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

# add_module_group("${ACCOUNTING_MODULES}") add_module_group("${MORE_MODULES}")

# Parse and add extra modules specified by the user
separate_arguments(INCLUDE_MODULES_LIST UNIX_COMMAND "${INCLUDE_MODULES}")
add_module_group("${INCLUDE_MODULES_LIST}")
message(STATUS "Extra modules: ${INCLUDE_MODULES_LIST}")
