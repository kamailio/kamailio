find_package(OpenSSL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(websocket SHARED ${SRC_FILES})
target_link_libraries(websocket PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# TODO: Verify if correct
if(EMBEDDED_UTF8_DECODE EQUAL 0)
  target_link_libraries(websocket PRIVATE unistring)
else()
  target_compile_definitions(websocket PRIVATE EMBEDDED_UTF8_DECODE)
endif()
