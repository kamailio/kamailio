cmake_minimum_required(VERSION 3.10)

project(kamailio_libs)

set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Each library is built as a separate target. The libraries are in the
# subdirectories of this folder and each subdirectory contains a CMakeLists.txt
# file that defines the library target.

file(
  GLOB children
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  LIST_DIRECTORIES true
  *
)

foreach(child ${children})
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${child}
     AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt
  )
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${child})

    install(
      TARGETS lib_${child}
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}
      COMPONENT kamailio_libs
    )
  endif()
endforeach()
