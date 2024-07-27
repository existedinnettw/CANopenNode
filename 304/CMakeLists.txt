add_library(304 INTERFACE)
target_sources(304 INTERFACE CO_GFC.c CO_SRDO.c)
target_include_directories(304 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(304 INTERFACE 301)
