add_library(co_storage INTERFACE)
target_sources(co_storage INTERFACE CO_storage.c # CO_storageEeprom.c
)
target_include_directories(co_storage INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(co_storage INTERFACE 301)

set(co_storage_PUBLIC_HEADERS "CO_storage.h")
set_target_properties(co_storage PROPERTIES PUBLIC_HEADER
                                            "${co_storage_PUBLIC_HEADERS}")
install(TARGETS co_storage DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/storage)

add_library(co_storage_eeprom INTERFACE)
target_sources(co_storage_eeprom INTERFACE CO_storageEeprom.c)
target_link_libraries(co_storage_eeprom INTERFACE co_storage)

set(CO_storageEeprom_PUBLIC_HEADERS "CO_storageEeprom.h" "CO_eeprom.h")
set_target_properties(
  co_storage_eeprom PROPERTIES PUBLIC_HEADER
                               "${CO_storageEeprom_PUBLIC_HEADERS}")
install(TARGETS co_storage_eeprom
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/storage)
