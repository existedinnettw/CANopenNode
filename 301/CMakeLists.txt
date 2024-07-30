# need user offer CO_driver_target.h to compile, so it should be interface
add_library(301 INTERFACE)
target_sources(
  301
  INTERFACE CO_Emergency.c
            CO_fifo.c
            CO_HBconsumer.c
            CO_NMT_Heartbeat.c
            CO_Node_Guarding.c
            CO_ODinterface.c
            CO_PDO.c
            CO_SDOclient.c
            CO_SDOserver.c
            CO_SYNC.c
            CO_TIME.c
            crc16-ccitt.c)

target_include_directories(301 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# CMake target_sources and install
set(301_PUBLIC_HEADERS
    "CO_Emergency.h"
    "CO_SYNC.h"
    "crc16-ccitt.h"
    "CO_SDOclient.h"
    "CO_HBconsumer.h"
    "CO_config.h"
    "CO_NMT_Heartbeat.h"
    "CO_Node_Guarding.h"
    "CO_driver.h"
    "CO_PDO.h"
    "CO_ODinterface.h"
    "CO_fifo.h"
    "CO_TIME.h"
    "CO_SDOserver.h")
set_target_properties(301 PROPERTIES PUBLIC_HEADER "${301_PUBLIC_HEADERS}")
install(TARGETS 301 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/301)
