add_library(309 INTERFACE)
target_sources(309 INTERFACE CO_gateway_ascii.c)
target_include_directories(309 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(309 INTERFACE 301 303 305)
