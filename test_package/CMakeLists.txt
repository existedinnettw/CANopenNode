cmake_minimum_required(VERSION 3.15)
project(PackageTest CXX C)

find_package(canopennode CONFIG REQUIRED)

# get_target_property(CO_SOURCES canopennode::canopennode INTERFACE_SOURCES)

# target_include_directories(canopennode::canopennode INTERFACE
# ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_package test_package.cpp # ${CO_SOURCES} # not work
)

# target_include_directories(canopennode::canopennode BEFORE INTERFACE
# ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(test_package PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_package PRIVATE canopennode::canopennode
                                           canopennode::co_storage)
