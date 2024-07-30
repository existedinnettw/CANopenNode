cmake_minimum_required(VERSION 3.23)
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

# https://discourse.cmake.org/t/how-to-install-an-interface-library-with-generated-source-in-build-folder/5449/2

set(CO_PUBLIC_HEADERS "CANopen.h")
set_target_properties(
  canopennode PROPERTIES PUBLIC_HEADER "${CO_PUBLIC_HEADERS}" RESOURCE
                                                              CANopen.c) #
# set_target_properties(canopennode PROPERTIES SOURCES CANopen.c)
target_sources(canopennode INTERFACE CANopen.c)
install(TARGETS canopennode)

# install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION
# ${CMAKE_INSTALL_PREFIX} FILES_MATCHING PATTERN "*.h" PATTERN "*.c" PATTERN
# "CMakeLists.txt")

add_subdirectory(storage)

if(BUILD_EXAMPLES)
  add_subdirectory(example)
endif()
