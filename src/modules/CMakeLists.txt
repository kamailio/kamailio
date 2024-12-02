
# Modules
set(MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Initialize a global property to track added modules
set_property(GLOBAL PROPERTY ADDED_MODULES_LIST "")

include(groups.cmake)

# Define lists of modules for each group
# message(STATUS "MODULE_GROUP_DEFAULT: ${MODULE_GROUP_DEFAULT}")

list(SORT MODULE_GROUP_DEFAULT)
message(STATUS "MODULE_GROUP_DEFAULT: ${MODULE_GROUP_DEFAULT}")

# Allow users to specify extra modules to build
set(INCLUDE_MODULES "" CACHE STRING "List of extra modules to build (space-separated)")

# Function to add modules from a list
function(add_module_group GROUP_MODULES)
  foreach(MODULE_NAME IN LISTS GROUP_MODULES)
  # Check if the module has already been added
    get_property(ALREADY_ADDED GLOBAL PROPERTY ADDED_MODULES_LIST)
    if(";${ALREADY_ADDED};" MATCHES ";${MODULE_NAME};")
        message(STATUS "Module ${MODULE_NAME} has already been added. Skipping...")
    else()
        # Construct the path to the module
        set(MODULE_PATH "${MODULES_DIR}/${MODULE_NAME}")
        # Check if the directory exists before adding
        if(IS_DIRECTORY ${MODULE_PATH} AND EXISTS ${MODULE_PATH}/CMakeLists.txt)
            add_subdirectory(${MODULE_PATH})
            # Remove the 'lib' prefix from the module name
            set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
            # Install the module to the appropriate directory under the installation prefix
            install(TARGETS ${MODULE_NAME} 
                    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/modules
                    COMPONENT kamailio_modules)
            # Add the module to the list of added modules
            set_property(GLOBAL APPEND PROPERTY ADDED_MODULES_LIST ${MODULE_NAME})
        else()
            message(WARNING "Module directory ${MODULE_PATH} does not exist.")
        endif()
    endif()
  endforeach()
endfunction()

# Add each group of modules
add_module_group("${MODULE_GROUP_DEFAULT}")
# add_module_group("${ACCOUNTING_MODULES}")
# add_module_group("${MORE_MODULES}")

# Parse and add extra modules specified by the user
separate_arguments(INCLUDE_MODULES_LIST UNIX_COMMAND "${INCLUDE_MODULES}")
add_module_group("${INCLUDE_MODULES_LIST}")
message(STATUS "Extra modules: ${INCLUDE_MODULES_LIST}")
