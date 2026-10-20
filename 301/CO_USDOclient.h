/**
 * CANopen FD Universal Service Data Object - client protocol.
 *
 * @file        CO_USDOclient.h
 * @ingroup     CO_USDOclient
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

#ifndef CO_USDO_CLIENT_H
#define CO_USDO_CLIENT_H

#include "301/CO_USDO.h"

#if (((CO_CONFIG_USDO)&CO_CONFIG_USDO_CLI_ENABLE) != 0) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_USDOclient USDO client
 * CANopen FD Universal Service Data Object - client protocol (CiA 1301).
 *
 * @ingroup CO_USDO
 * @{
 * USDO client reads (upload) or writes (download) OD variables on USDO server. Transfer is started with
 * CO_USDOclient_download() or CO_USDOclient_upload() and then CO_USDOclient_process() must be called cyclically, until
 * it returns value different than @ref CO_USDO_RT_waitingResponse. Expedited transfer is used for data up to 56 bytes,
 * otherwise segmented transfer is used. Data buffer, passed to download or upload function, must stay valid until the
 * end of the transfer.
 *
 * Example:
 * @code{.c}
uint32_t value = 0x12345678;
CO_USDOclient_download(USDOclient, 5, 0x2000, 0, CO_USDO_DT_UNSIGNED32, &value, sizeof(value), 500);
do {
    ret = CO_USDOclient_process(USDOclient, timeDifference_us, NULL, &abortCode, NULL);
    sleep(...);
} while (ret == CO_USDO_RT_waitingResponse);
 * @endcode
 *
 * Multi-byte values are transferred in little-endian byte order, as they are in CANopen. Data buffer is not modified
 * by USDO client on big-endian systems.
 */

/**
 * Return values of USDO client functions
 */
typedef enum {
    CO_USDO_RT_waitingResponse = 1,        /**< Waiting for the server response */
    CO_USDO_RT_ok_communicationEnd = 0,    /**< Transfer finished successfully */
    CO_USDO_RT_wrongArguments = -2,        /**< Error in arguments */
    CO_USDO_RT_busy = -3,                  /**< Another transfer is in progress */
    CO_USDO_RT_endedWithClientAbort = -9,  /**< Transfer ended with client abort (also on timeout) */
    CO_USDO_RT_endedWithServerAbort = -10, /**< Transfer ended with server abort */
} CO_USDO_return_t;

/**
 * Internal state of the USDO client
 */
typedef enum {
    CO_USDO_CLI_ST_IDLE = 0x00U,    /**< No transfer */
    CO_USDO_CLI_ST_DL_EXP = 0x01U,  /**< Waiting for expedited download response */
    CO_USDO_CLI_ST_DL_INIT = 0x02U, /**< Waiting for segmented download initiate response */
    CO_USDO_CLI_ST_DL_SEG = 0x03U,  /**< Waiting for download segment response */
    CO_USDO_CLI_ST_DL_END = 0x04U,  /**< Waiting for download end response */
    CO_USDO_CLI_ST_UL_INIT = 0x05U, /**< Waiting for upload (expedited or segmented initiate) response */
    CO_USDO_CLI_ST_UL_SEG = 0x06U   /**< Waiting for upload segment response */
} CO_USDO_clientState_t;

/**
 * USDO client object.
 */
typedef struct {
    CO_CANmodule_t* CANdevTx;      /**< From CO_USDOclient_init() */
    uint16_t CANdevTxIdx;          /**< From CO_USDOclient_init() */
    CO_CANtx_t* CANtxBuff;         /**< CAN transmit buffer inside CANdevTx */
    uint8_t nodeId;                /**< Node-ID of this client, from CO_USDOclient_init() */
    CO_USDO_clientState_t state;   /**< Internal state */
    uint8_t serverNodeId;          /**< Node-ID of the server in current transfer */
    uint8_t sessionId;             /**< Session-ID of the current transfer, 1 to 255 */
    uint16_t index;                /**< Index of the OD variable in current transfer */
    uint8_t subIndex;              /**< Sub-index of the OD variable in current transfer */
    uint8_t dataType;              /**< Data type, indicated by download or received by upload */
    uint8_t segCounter;            /**< Segment counter */
    const uint8_t* dataDownload;   /**< Data buffer for download */
    uint8_t* dataUpload;           /**< Data buffer for upload */
    size_t bufSize;                /**< Size of data for download or size of buffer for upload */
    size_t sizeInd;                /**< Size of data indicated by server in upload, 0 if unknown */
    size_t sizeTran;               /**< Size of transferred data */
    uint32_t timeoutTime_us;       /**< Timeout time for response */
    uint32_t timeoutTimer;         /**< Timeout timer */
    CO_USDO_abortCode_t abortCode; /**< Abort code of finished transfer */
    volatile void* CANrxNew;       /**< Indicates, if new response is received */
    uint8_t CANrxData[64];         /**< Data of the received response */
    uint8_t CANrxLen;              /**< Data length of the received response */
#if (((CO_CONFIG_USDO)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0) || defined CO_DOXYGEN
    void (*pFunctSignalPre)(void* object); /**< From CO_USDOclient_initCallbackPre() or NULL */
    void* functSignalObjectPre;            /**< From CO_USDOclient_initCallbackPre() or NULL */
#endif
} CO_USDOclient_t;

/**
 * Initialize USDO client object.
 *
 * Function must be called in the communication reset section.
 *
 * @param USDO_C This object will be initialized.
 * @param nodeId CANopen Node ID of this device.
 * @param CANdevRx CAN device for USDO client reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device for USDO client transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_USDOclient_init(CO_USDOclient_t* USDO_C, uint8_t nodeId, CO_CANmodule_t* CANdevRx,
                                    uint16_t CANdevRxIdx, CO_CANmodule_t* CANdevTx, uint16_t CANdevTxIdx);

#if (((CO_CONFIG_USDO)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0) || defined CO_DOXYGEN
/**
 * Initialize USDO client callback function.
 *
 * Function initializes optional callback function, which should immediately start processing of
 * CO_USDOclient_process() function. Callback is called after USDO client CAN frame is received from the CAN bus.
 *
 * @param USDO_C This object.
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can be NULL
 * @param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_USDOclient_initCallbackPre(CO_USDOclient_t* USDO_C, void* object, void (*pFunctSignalPre)(void* object));
#endif

/**
 * Start USDO download (write OD variable on the server).
 *
 * @param USDO_C This object.
 * @param serverNodeId Node-ID of the USDO server, 1 to 127.
 * @param index Index of the OD variable.
 * @param subIndex Sub-index of the OD variable.
 * @param dataType Data type, see @ref CO_USDO_dataType_t.
 * @param data Data to be written, must stay valid until the end of the transfer.
 * @param size Size of data in bytes.
 * @param timeoutTime_ms Timeout time for server response in milliseconds.
 *
 * @return CO_USDO_RT_waitingResponse on success, CO_USDO_RT_wrongArguments or CO_USDO_RT_busy.
 */
CO_USDO_return_t CO_USDOclient_download(CO_USDOclient_t* USDO_C, uint8_t serverNodeId, uint16_t index, uint8_t subIndex,
                                        uint8_t dataType, const void* data, size_t size, uint16_t timeoutTime_ms);

/**
 * Start USDO upload (read OD variable from the server).
 *
 * @param USDO_C This object.
 * @param serverNodeId Node-ID of the USDO server, 1 to 127.
 * @param index Index of the OD variable.
 * @param subIndex Sub-index of the OD variable.
 * @param buf Buffer for data, must stay valid until the end of the transfer.
 * @param bufSize Size of the buffer. If server sends more data, transfer is aborted with @ref CO_USDO_AB_OUT_OF_MEM.
 * @param timeoutTime_ms Timeout time for server response in milliseconds.
 *
 * @return CO_USDO_RT_waitingResponse on success, CO_USDO_RT_wrongArguments or CO_USDO_RT_busy.
 */
CO_USDO_return_t CO_USDOclient_upload(CO_USDOclient_t* USDO_C, uint8_t serverNodeId, uint16_t index, uint8_t subIndex,
                                      void* buf, size_t bufSize, uint16_t timeoutTime_ms);

/**
 * Process USDO client.
 *
 * Function must be called cyclically during transfer.
 *
 * @param USDO_C This object.
 * @param timeDifference_us Time difference from previous function call in [microseconds].
 * @param [out] timerNext_us info to OS - see CO_process(), may be NULL.
 * @param [out] abortCode Abort code, if transfer ended with abort, may be NULL.
 * @param [out] sizeTransferred Number of bytes transferred, may be NULL. For upload this is the size of data in buf.
 *
 * @return #CO_USDO_return_t: CO_USDO_RT_waitingResponse while transfer is in progress, CO_USDO_RT_ok_communicationEnd
 * or one of the abort codes at the end.
 */
CO_USDO_return_t CO_USDOclient_process(CO_USDOclient_t* USDO_C, uint32_t timeDifference_us, uint32_t* timerNext_us,
                                       CO_USDO_abortCode_t* abortCode, size_t* sizeTransferred);

/**
 * Abort current USDO transfer.
 *
 * @param USDO_C This object.
 * @param abortCode Abort code sent to the server.
 */
void CO_USDOclient_abort(CO_USDOclient_t* USDO_C, CO_USDO_abortCode_t abortCode);

/** @} */ /* CO_USDOclient */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_USDO) & CO_CONFIG_USDO_CLI_ENABLE */

#endif /* CO_USDO_CLIENT_H */
