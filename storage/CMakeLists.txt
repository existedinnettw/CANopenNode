add_library(co_storage INTERFACE)
target_sources(co_storage INTERFACE CO_storage.c # CO_storageEeprom.c
)
target_include_directories(co_storage INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(co_storage INTERFACE 301)

add_library(co_storage_eeprom INTERFACE)
target_sources(co_storage_eeprom INTERFACE CO_storageEeprom.c)
target_link_libraries(co_storage_eeprom INTERFACE co_storage)
