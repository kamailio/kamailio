cmake_minimum_required(VERSION 3.10)

file(GLOB kamailio_utils_SRC 
    "utils/*.c"
    )
add_library(utils ${kamailio_utils_SRC})
target_include_directories(utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/utils")
target_link_libraries(utils PUBLIC common m)

file(GLOB kamailio_cfg_SRC
    "cfg/*.c"
    "ppcfg.c"
    "cfg_tab.c"
    "lex.yy.c"
    )
add_library(cfg ${kamailio_cfg_SRC})
target_include_directories(cfg PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/cfg")
target_link_libraries(cfg PUBLIC kamailio_core)
add_dependencies(cfg GenerateParser)

file(GLOB kamailio_memory_SRC 
    "mem/*.c"
    "pt.c"
    # "sr_module.c"
    # "mod_fix.c"
    )
# message(STATUS "kamailio_memory_SRC: ${kamailio_memory_SRC}")
add_library(memory ${kamailio_memory_SRC})
target_include_directories(memory PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mem")
target_include_directories(memory PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(memory PUBLIC common cfg kamailio_core)

file(GLOB kamailio_crypto_SRC 
    "crypto/*.c"
)

add_library(crypto ${kamailio_crypto_SRC})
target_include_directories(crypto PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/crypto")
target_link_libraries(crypto PUBLIC common)

file(GLOB kamailio_rand_SRC 
    "rand/**/*.c"
    "rand/*.c"
    )
add_library(rand ${kamailio_rand_SRC})
target_link_libraries(rand PUBLIC common logging memory crypto)

set(kamailio_logging_SRC 
        "dprint.c"
        "error.c"
        )

add_library(logging ${kamailio_logging_SRC})
target_link_libraries(logging PUBLIC common parser)


file(GLOB kamailio_parser_SRC
    "parser/**/*.c"    
    "parser/*.c"
)
# message(STATUS "kamailio_parser_SRC: ${kamailio_parser_SRC}")
add_library(parser ${kamailio_parser_SRC})
target_link_libraries(parser PUBLIC common logging memory kamailio_core)

file(GLOB kamailio_tcp_main_SRC 
    "tcp_main.c"
    # "tcp_main.h"
    "tcp_options.c"
    )
add_library(tcp_main ${kamailio_tcp_main_SRC})
target_link_libraries(tcp_main PUBLIC common logging memory)

# Set sources found on this folder
# TODO: Exlude already compiled files that are part of other targets?
file(GLOB kamailio_core_SRC 
    "*.c"
    )
# exclude some files files found above
# list(FILTER kamailio_core_SRC EXCLUDE REGEX ".*mem.*")
# list(FILTER kamailio_core_SRC EXCLUDE REGEX ".*rand.*")
# list(FILTER kamailio_core_SRC EXCLUDE REGEX ".*crypto.*")
# list(FILTER kamailio_core_SRC EXCLUDE REGEX ".*pt.*")
# list(FILTER kamailio_core_SRC EXCLUDE REGEX ".*parser.*")
# list(FILTER kamailio_core_SRC EXCLUDE REGEX ".*tcp_main.*")
# list(FILTER kamailio_core_SRC EXCLUDE REGEX ".*dprint.*")
# list(FILTER kamailio_core_SRC EXCLUDE REGEX ".*error.*")

# message(STATUS "kamailio_core_SRC: ${kamailio_core_SRC}")

get_target_property(MY_INTERFACE_DEFINITIONS common INTERFACE_COMPILE_DEFINITIONS)
message(STATUS "Compile definitions for common: ${MY_INTERFACE_DEFINITIONS}")

add_library(kamailio_core ${kamailio_core_SRC})
target_include_directories(kamailio_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kamailio_core PUBLIC common cfg tcp_main logging memory parser rand utils 
                                        Threads::Threads # Pthread 
                                        dl      # dlopen and more
                                        resolv  # resolv
                                        )