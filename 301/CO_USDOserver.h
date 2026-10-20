/**
 * CANopen FD Universal Service Data Object - server protocol.
 *
 * @file        CO_USDOserver.h
 * @ingroup     CO_USDOserver
 *
 * This file is part of <https://github.com/CANopenNode/CANopenNode>, a CANopen Stack.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */

#ifndef CO_USDO_SERVER_H
#define CO_USDO_SERVER_H

#include "301/CO_USDO.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_USDO_SRV_BUFFER_SIZE
#define CO_CONFIG_USDO_SRV_BUFFER_SIZE 128
#endif
#ifndef CO_CONFIG_USDO_SRV_RX_FIFO
#define CO_CONFIG_USDO_SRV_RX_FIFO 4
#endif
#ifndef CO_CONFIG_USDO_SRV_BULK_TIME
#define CO_CONFIG_USDO_SRV_BULK_TIME 10
#endif

#if (((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_ENABLE) != 0) || defined CO_DOXYGEN

#if CO_CONFIG_USDO_SRV_BUFFER_SIZE < 64
#error CO_CONFIG_USDO_SRV_BUFFER_SIZE must be at least 64
#endif
#if (CO_CONFIG_USDO_SRV_RX_FIFO < 1) || (CO_CONFIG_USDO_SRV_RX_FIFO > 255)
#error CO_CONFIG_USDO_SRV_RX_FIFO is out of range
#endif
#if (((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_BULK) != 0) && (((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_SEGMENTED) == 0)
#error CO_CONFIG_USDO_SRV_BULK requires CO_CONFIG_USDO_SRV_SEGMENTED
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_USDOserver USDO server
 * CANopen FD Universal Service Data Object - server protocol (CiA 1301).
 *
 * @ingroup CO_USDO
 * @{
 * USDO server provides access to the Object Dictionary for USDO clients. It supports:
 * - expedited download and upload (up to 56 bytes),
 * - segmented download and upload (60 bytes per segment), if @ref CO_CONFIG_USDO_SRV_SEGMENTED is enabled,
 * - bulk download (unconfirmed segments), if @ref CO_CONFIG_USDO_SRV_BULK is enabled,
 * - broadcast requests (destination address 0) for expedited download, expedited upload and bulk download.
 *
 * Server receives requests from any client (CAN-ID 0x600 to 0x67F) and handles one transfer at a time. Request from
 * another client during transfer is aborted with @ref CO_USDO_AB_BUSY. Received CAN frames are stored into a small
 * fifo (@ref CO_CONFIG_USDO_SRV_RX_FIFO) and processed by CO_USDOserver_process().
 *
 * Data type in the upload response is determined from the attributes and length of the OD variable: @ref ODA_STR
 * gives VISIBLE_STRING, multi-byte variables give UNSIGNED16/24/32/64, one byte variables UNSIGNED8, other OCTET_STRING
 * or DOMAIN. Application may specify exact data types with CO_USDOserver_initCallbackDataType(). Data type in the
 * download request is not verified, only data length is verified.
 */

/**
 * Internal state of the USDO server
 */
typedef enum {
    CO_USDO_ST_IDLE = 0x00U,    /**< Waiting for the client request */
    CO_USDO_ST_DL_SEG = 0x01U,  /**< Segmented download in progress */
    CO_USDO_ST_DL_BULK = 0x02U, /**< Bulk download in progress */
    CO_USDO_ST_UL_SEG = 0x03U   /**< Segmented upload in progress */
} CO_USDO_state_t;

/**
 * USDO server object.
 */
typedef struct {
    CO_CANmodule_t* CANdevTx;                            /**< From CO_USDOserver_init() */
    uint16_t CANdevTxIdx;                                /**< From CO_USDOserver_init() */
    CO_CANtx_t* CANtxBuff;                               /**< CAN transmit buffer inside CANdevTx */
    OD_t* OD;                                            /**< From CO_USDOserver_init() */
    uint8_t nodeId;                                      /**< From CO_USDOserver_init() */
    CO_USDO_state_t state;                               /**< Internal state of the USDO server */
    OD_IO_t OD_IO;                                       /**< Object dictionary interface for current object. */
    uint16_t index;                                      /**< Index of the current object in Object Dictionary */
    uint8_t subIndex;                                    /**< Subindex of the current object in Object Dictionary */
    uint8_t clientNodeId;                                /**< Node-ID of the client in current transfer */
    uint8_t sessionId;                                   /**< Session-ID of the current transfer */
    uint8_t segCounter;                                  /**< Expected segment counter */
    volatile void* CANrxNew[CO_CONFIG_USDO_SRV_RX_FIFO]; /**< Indicates, if fifo slot contains received frame */
    uint8_t CANrxData[CO_CONFIG_USDO_SRV_RX_FIFO][64];   /**< Data of the received frames */
    uint8_t CANrxLen[CO_CONFIG_USDO_SRV_RX_FIFO];        /**< Data length of the received frames */
    uint8_t CANrxClient[CO_CONFIG_USDO_SRV_RX_FIFO];     /**< Node-ID of the client, which sent the frame */
    uint8_t CANrxWr;                                     /**< Fifo write index, used by receive function */
    uint8_t CANrxRd;                                     /**< Fifo read index, used by processing function */
    volatile bool_t CANrxOverflow;                       /**< Set if received frame was lost because of full fifo */
    uint32_t timeoutTime_us;                             /**< Maximum time between frames inside segmented transfer */
    uint32_t timeoutTimer;                               /**< Timeout timer for USDO communication */
    OD_size_t sizeInd;  /**< Size of data indicated by client (download) or server (upload), 0 if unknown */
    OD_size_t sizeTran; /**< Size of data which is actually transferred */
    bool_t finished;    /**< Data transfer is finished (download) or read from OD is finished (upload) */
    uint8_t buf[CO_CONFIG_USDO_SRV_BUFFER_SIZE + 1U]; /**< Interim data buffer + byte for '\0' */
    OD_size_t bufOffsetWr; /**< Offset of next free data byte available for write in the buffer. */
    OD_size_t bufOffsetRd; /**< Offset of first data available for read in the buffer */
    uint8_t (*pFunctGetDataType)(void* object, uint16_t index, uint8_t subIndex,
                                 const OD_stream_t* stream); /**< From CO_USDOserver_initCallbackDataType() or NULL */
    void* functGetDataTypeObject;                            /**< From CO_USDOserver_initCallbackDataType() or NULL */
#if (((CO_CONFIG_USDO)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0) || defined CO_DOXYGEN
    void (*pFunctSignalPre)(void* object); /**< From CO_USDOserver_initCallbackPre() or NULL */
    void* functSignalObjectPre;            /**< From CO_USDOserver_initCallbackPre() or NULL */
#endif
} CO_USDOserver_t;

/**
 * Initialize USDO server object.
 *
 * Function must be called in the communication reset section.
 *
 * @param USDO This object will be initialized.
 * @param OD Object Dictionary.
 * @param nodeId CANopen Node ID of this device.
 * @param timeoutTime_ms Timeout time for segmented transfer in milliseconds.
 * @param CANdevRx CAN device for USDO server reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device for USDO server transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_USDOserver_init(CO_USDOserver_t* USDO, OD_t* OD, uint8_t nodeId, uint16_t timeoutTime_ms,
                                    CO_CANmodule_t* CANdevRx, uint16_t CANdevRxIdx, CO_CANmodule_t* CANdevTx,
                                    uint16_t CANdevTxIdx, uint32_t* errInfo);

#if (((CO_CONFIG_USDO)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0) || defined CO_DOXYGEN
/**
 * Initialize USDO server callback function.
 *
 * Function initializes optional callback function, which should immediately start processing of
 * CO_USDOserver_process() function. Callback is called after USDO server CAN frame is received from the CAN bus.
 *
 * @param USDO This object.
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can be NULL
 * @param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_USDOserver_initCallbackPre(CO_USDOserver_t* USDO, void* object, void (*pFunctSignalPre)(void* object));
#endif

/**
 * Initialize callback function, which determines data type of the OD variable.
 *
 * Data type is sent in USDO upload response. If callback is not set or it returns @ref CO_USDO_DT_UNKNOWN, then data
 * type is determined from attributes and length of the OD variable.
 *
 * @param USDO This object.
 * @param object Pointer to object, which will be passed to pFunctGetDataType(). Can be NULL
 * @param pFunctGetDataType Pointer to the callback function, which returns @ref CO_USDO_dataType_t.
 */
void CO_USDOserver_initCallbackDataType(CO_USDOserver_t* USDO, void* object,
                                        uint8_t (*pFunctGetDataType)(void* object, uint16_t index, uint8_t subIndex,
                                                                     const OD_stream_t* stream));

/**
 * Process USDO server.
 *
 * Function must be called cyclically.
 *
 * @param USDO This object.
 * @param NMTisPreOrOperational True if this node is in NMT_PRE_OPERATIONAL or NMT_OPERATIONAL state.
 * @param timeDifference_us Time difference from previous function call in [microseconds].
 * @param [out] timerNext_us info to OS - see CO_process().
 *
 * @return true, if transfer is in progress (more processing is expected).
 */
bool_t CO_USDOserver_process(CO_USDOserver_t* USDO, bool_t NMTisPreOrOperational, uint32_t timeDifference_us,
                             uint32_t* timerNext_us);

/** @} */ /* CO_USDOserver */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_USDO) & CO_CONFIG_USDO_SRV_ENABLE */

#endif /* CO_USDO_SERVER_H */
