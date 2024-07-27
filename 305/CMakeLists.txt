add_library(305 INTERFACE)
target_sources(305 INTERFACE CO_LSSmaster.c CO_LSSslave.c)
target_include_directories(305 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(305 INTERFACE 301)
