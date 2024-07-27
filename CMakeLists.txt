cmake_minimum_required(VERSION 3.15)
project(canopennode C)

add_subdirectory(301)
add_subdirectory(303)
add_subdirectory(304)
add_subdirectory(305)
add_subdirectory(309)
add_subdirectory(extra)

add_library(canopennode INTERFACE)
target_sources(canopennode INTERFACE CANopen.c)
target_include_directories(canopennode INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(canopennode INTERFACE 301 303 304 305 309 extra)

add_subdirectory(storage)

if(BUILD_EXAMPLES)
  add_subdirectory(example)
endif()
