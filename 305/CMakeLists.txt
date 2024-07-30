add_library(305 INTERFACE)
target_sources(305 INTERFACE CO_LSSmaster.c CO_LSSslave.c)
target_include_directories(305 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(305 INTERFACE 301)

set(305_PUBLIC_HEADERS "CO_LSSslave.h" "CO_LSS.h" "CO_LSSmaster.h")
set_target_properties(305 PROPERTIES PUBLIC_HEADER "${305_PUBLIC_HEADERS}")
install(TARGETS 305 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/305)
