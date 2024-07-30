add_library(extra INTERFACE CO_trace.c)
target_include_directories(extra INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(extra INTERFACE 301)

set(extra_PUBLIC_HEADERS "CO_trace.h")
set_target_properties(extra PROPERTIES PUBLIC_HEADER "${extra_PUBLIC_HEADERS}")
install(TARGETS extra DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/extra)
