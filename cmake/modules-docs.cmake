option(BUILD_DOC "Build documentation" ON)

# Readme file and man page
find_program(XSLTPROC_EXECUTABLE xsltproc QUIET)
find_program(LYNX_EXECUTABLE lynx QUIET)

if(BUILD_DOC
   AND NOT XSLTPROC_EXECUTABLE
   AND NOT LYNX_EXECUTABLE
)
  message(
    STATUS
      "xsltproc or lynx not found but required for doc generation. Disabling documentation build."
  )
  set(BUILD_DOC OFF)
else()
  option(DOCS_XSL_VAIDATION "Docbook document validation" OFF)
  option(DOCS_NOCATALAOG
         "ON: Use standard catalog from OS/ OFF: USE custom catalog " ON
  )
  set(DOCS_SOURCES
      "index.html"
      CACHE STRING "Documentation files list"
  )
  set(DOCS_README
      ${DOCS_SOURCES}
      CACHE STRING "Readme Documentation files list"
  )
  set(DOCS_HTML
      ${DOCS_SOURCES}
      CACHE STRING "HTML Documentation files list"
  )
  set(DOCS_TXT
      ${DOCS_SOURCES}
      CACHE STRING "TXT Documentation files list"
  )

  set(DOCS_OUTPUT_DIR
      ${CMAKE_BINARY_DIR}/doc/docbook
      CACHE STRING "Path to build HTML docs"
  )

  set(DOCBOOK_DIR ${CMAKE_SOURCE_DIR}/doc/docbook)
  set(DEPS_XSL ${DOCBOOK_DIR}/dep.xsl)
  set(SINGLE_HTML_XSL ${DOCBOOK_DIR}/html.xsl)
  set(CHUNKED_HTML_XSL ${DOCBOOK_DIR}/html.chunked.xsl)
  set(TXT_XSL ${DOCBOOK_DIR}/txt.xsl)
  set(README_XSL ${DOCBOOK_DIR}/readme.xsl)

  set(DOCS_HTML_CSS
      "/css/sr-doc.css"
      CACHE STRING "Path to the CSS file"
  )

  set(CATALOG ${DOCBOOK_DIR}/catalog.xml)

  if(DOCS_NOCATALAOG)
    set(XMLCATATLOGX "")
  else()
    set(XMLCATATLOGX XML_CATALOG_FILES=${CATALOG})
  endif()

  # Set flags for xtproc for generating documentation and allow user defined
  set(DOCS_XSLTPROC_FLAGS
      ""
      CACHE STRING "Xsltransform processor flags"
  )
  if(NOT DOCS_XSL_VAIDATION)
    message(STATUS "Disabling validation when generating documentation")
    list(APPEND DOCS_XSLTPROC_FLAGS --novalid)
  endif()

  # Set lynx flags for generating readmes and allow user defined
  set(DOCS_LYNX_FLAGS
      "-nolist"
      CACHE STRING "Lynx readme generator flags"
  )

  # Function to add a module docs entry
  function(docs_add_module MODULE_NAME)
    # message(STATUS "Adding documentation for module ${MODULE_NAME}")
    set(MODULE_PATH "${MODULES_DIR}/${MODULE_NAME}")
    set(MODULE_DOC_PATH "${MODULE_PATH}/doc")

    add_custom_target(
      ${MODULE_NAME}_doc_text
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
      COMMENT "Processing target ${MODULE_NAME}_doc_text"
    )

    add_custom_target(
      ${MODULE_NAME}_readme
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/README
      COMMENT "Processing target ${MODULE_NAME}_readme"
    )

    add_custom_target(
      ${MODULE_NAME}_doc_html
      DEPENDS ${DOCS_OUTPUT_DIR}/${MODULE_NAME}.html
      COMMENT "Processing target ${MODULE_NAME}_doc_html"
    )

    add_custom_target(
      ${MODULE_NAME}_doc
      DEPENDS ${MODULE_NAME}_doc_text ${MODULE_NAME}_doc_html
      COMMENT "Processing target ${MODULE_NAME}_doc"
    )

    # Each version has seperate custon coommands for not recompiling all if 1
    # gets changed.
    if(XSLTPROC_EXECUTABLE)
      if(LYNX_EXECUTABLE)
        add_custom_command(
          OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
          COMMAND
            # TXT version - just plain text
            ${XMLCATATLOGX} ${XSLTPROC_EXECUTABLE} ${DOCS_XSLTPROC_FLAGS}
            --xinclude ${TXT_XSL} ${MODULE_NAME}.xml | ${LYNX_EXECUTABLE}
            ${DOCS_LYNX_FLAGS} -stdin -dump >
            ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
          DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/doc/${MODULE_NAME}.xml
            ${TXT_XSL}
            # ${SINGLE_HTML_XSL}
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/doc
          COMMENT
            "Generating text documentation with xsltproc and lynx for ${MODULE_NAME}"
        )

        add_custom_command(
          OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/README
          COMMAND
            ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/README
          DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
          COMMENT "Generating README file for module ${MODULE_NAME}"
        )

        add_custom_command(
          OUTPUT ${DOCS_OUTPUT_DIR}/${MODULE_NAME}.html
          COMMAND
            # HTML version
            ${XMLCATATLOGX} ${XSLTPROC_EXECUTABLE} ${DOCS_XSLTPROC_FLAGS}
            --xinclude --stringparam base.dir ${DOCS_OUTPUT_DIR} --stringparam
            root.filename ${MODULE_NAME} --stringparam html.stylesheet
            ${DOCS_HTML_CSS} --stringparam html.ext ".html" ${SINGLE_HTML_XSL}
            ${MODULE_NAME}.xml
          DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/doc/${MODULE_NAME}.xml
            ${SINGLE_HTML_XSL}
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/doc
          COMMENT
            "Generating html documentation with xsltproc and lynx for ${MODULE_NAME}"
        )
      endif()

      install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/${MODULE_NAME}.txt
        RENAME README.${MODULE_NAME}
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/modules
        COMPONENT kamailio_docs
        # Since the depepndencies might not have been build as they are not in the default target
        # and required to build explicitly using `make kamailio_docs`, allow them to be optional.
        OPTIONAL
      )
    endif()
  endfunction()
endif()
