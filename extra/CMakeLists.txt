add_library(extra INTERFACE CO_trace.c)
target_include_directories(extra INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(extra INTERFACE 301)
