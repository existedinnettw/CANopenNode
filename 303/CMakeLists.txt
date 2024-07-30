add_library(303 INTERFACE)
target_sources(303 INTERFACE CO_LEDs.c)
target_include_directories(303 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# add_library(303 CO_LEDs.c) target_include_directories(303 PUBLIC
# ${CMAKE_SOURCE_DIR})

target_link_libraries(303 INTERFACE 301)

set(303_PUBLIC_HEADERS "CO_LEDs.h")
set_target_properties(303 PROPERTIES PUBLIC_HEADER "${303_PUBLIC_HEADERS}")
install(TARGETS 303 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/303)
