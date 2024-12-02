option(BUILD_DOC "Build documentation" ON)

# Readme file and man page
find_program(XSLTPROC_EXECUTABLE xsltproc QUIET)
find_program(PANDOC_EXECUTABLE pandoc QUIET)

# Function to add a module docs entry
function(docs_add_module MODULE_NAME)
  # message(STATUS "Adding documentation for module ${MODULE_NAME}")
  if(BUILD_DOC)

    set(MODULE_PATH "${MODULES_DIR}/${MODULE_NAME}")
    set(MODULE_DOC_PATH "${MODULE_PATH}/doc")
    # Check if the module has a 'doc' directory and if it contains a file named
    # MODULE_NAME.xml

    if(XSLTPROC_EXECUTABLE)

      add_custom_command(
        OUTPUT # ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.md
               # ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
               ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/README
        # ${MODULE_NAME}_doc The following command is used to generate the
        # documentation in html format from the xml file
        COMMAND
          ${XSLTPROC_EXECUTABLE} --novalid --xinclude
          # -o ${CMAKE_CURRENT_BINARY_DIR}/xprint2.xml
          ${CMAKE_SOURCE_DIR}/doc/docbook/html.xsl
          ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/doc/${MODULE_NAME}.xml
        COMMAND
          ${PANDOC_EXECUTABLE} -s -f html -t markdown_strict --output
          ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.md
          ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/index.html
        COMMAND
          ${PANDOC_EXECUTABLE} -s -f html -t plain --output
          # ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/README
          ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
          ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/index.html
        COMMAND
          ${CMAKE_COMMAND} -E copy
          ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
          ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/README
        DEPENDS
          ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/doc/${MODULE_NAME}.xml
          ${CMAKE_SOURCE_DIR}/doc/docbook/html.xsl
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}
        COMMENT "Generating documentation with xsltproc for ${MODULE_NAME}"
      )

      add_custom_target(
        ${MODULE_NAME}_doc
        DEPENDS # ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.md
                # ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
                ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/README
      )

      install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/README
        RENAME README.${MODULE_NAME}
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/modules
        COMPONENT kamailio_docs
        # OPTIONAL
      )

      # if(IS_DIRECTORY ${MODULE_DOC_PATH} AND EXISTS
      # ${MODULE_DOC_PATH}/${MODULE_NAME}.xml ) install( FILES
      # ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.md DESTINATION
      # ${CMAKE_INSTALL_DOCDIR}/modules COMPONENT kamailio-docs OPTIONAL ) #
      # endif()

      # endif()
    endif()
  endif()
endfunction()
