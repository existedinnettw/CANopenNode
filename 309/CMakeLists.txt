add_library(309 INTERFACE)
target_sources(309 INTERFACE CO_gateway_ascii.c)
target_include_directories(309 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(309 INTERFACE 301 303 305)

set(309_PUBLIC_HEADERS "CO_gateway_ascii.h")
set_target_properties(309 PROPERTIES PUBLIC_HEADER "${309_PUBLIC_HEADERS}")
install(TARGETS 309 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/309)
