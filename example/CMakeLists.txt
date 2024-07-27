add_executable(main_blank main_blank.c OD.c CO_driver_blank.c CO_storageBlank.c)

target_link_directories(canopennode INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(main_blank PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(main_blank PRIVATE canopennode co_storage)
