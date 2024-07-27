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
            crc16-ccitt.c
            # "${CO_DRIVER_TARGET_SRC_DIR}/CO_driver_target.h"
)

# message("\n\nexample driver target source2: ${CO_DRIVER_TARGET_SRC_DIR}")
target_include_directories(301 INTERFACE # ${CO_DRIVER_TARGET_SRC_DIR}
                                         ${CMAKE_CURRENT_SOURCE_DIR})
