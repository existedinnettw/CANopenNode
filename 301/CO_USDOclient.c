/*
 * CANopen FD Universal Service Data Object - client protocol.
 *
 * @file        CO_USDOclient.c
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

#include <string.h>

#include "301/CO_USDOclient.h"

#if ((CO_CONFIG_USDO)&CO_CONFIG_USDO_CLI_ENABLE) != 0

/*
 * Read received frame from CAN module
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN frame with correct identifier will be
 * received. For more information and description of parameters see file CO_driver.h.
 */
static void
CO_USDOclient_receive(void* object, void* msg) {
    CO_USDOclient_t* USDO_C = (CO_USDOclient_t*)object;
    uint8_t serverNodeId = (uint8_t)(CO_CANrxMsg_readIdent(msg) & 0x7FU);
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    const uint8_t* data = CO_CANrxMsg_readData(msg);

    /* accept only response to the current request */
    if ((USDO_C->state == CO_USDO_CLI_ST_IDLE) || (serverNodeId != USDO_C->serverNodeId) || (DLC < 3U)
        || (data[0] != USDO_C->nodeId) || (data[2] != USDO_C->sessionId) || CO_FLAG_READ(USDO_C->CANrxNew)) {
        return;
    }

    if (DLC > 64U) {
        DLC = 64U;
    }
    (void)memcpy((void*)USDO_C->CANrxData, (const void*)data, DLC);
    USDO_C->CANrxLen = DLC;
    CO_FLAG_SET(USDO_C->CANrxNew);

#if ((CO_CONFIG_USDO)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    /* Optional signal to RTOS, which can resume task, which handles USDO client. */
    if (USDO_C->pFunctSignalPre != NULL) {
        USDO_C->pFunctSignalPre(USDO_C->functSignalObjectPre);
    }
#endif
}

CO_ReturnError_t
CO_USDOclient_init(CO_USDOclient_t* USDO_C, uint8_t nodeId, CO_CANmodule_t* CANdevRx, uint16_t CANdevRxIdx,
                   CO_CANmodule_t* CANdevTx, uint16_t CANdevTxIdx) {
    if ((USDO_C == NULL) || (CANdevRx == NULL) || (CANdevTx == NULL) || (nodeId < 1U) || (nodeId > 127U)) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    (void)memset((void*)USDO_C, 0, sizeof(CO_USDOclient_t));
    USDO_C->nodeId = nodeId;
    USDO_C->CANdevTx = CANdevTx;
    USDO_C->CANdevTxIdx = CANdevTxIdx;
    USDO_C->state = CO_USDO_CLI_ST_IDLE;

    /* receive responses from all servers: CAN-ID 0x580 to 0x5FF */
    CO_ReturnError_t ret = CO_CANrxBufferInit(CANdevRx, CANdevRxIdx, (uint16_t)CO_CAN_ID_USDO_RSP, 0x780U, false,
                                              (void*)USDO_C, CO_USDOclient_receive);
    if (ret != CO_ERROR_NO) {
        return ret;
    }

    USDO_C->CANtxBuff = CO_CANtxBufferInit(CANdevTx, CANdevTxIdx, (uint16_t)CO_CAN_ID_USDO_REQ + nodeId, false,
                                           CO_USDO_SEGMENT_LEN, false);
    if (USDO_C->CANtxBuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return CO_ERROR_NO;
}

#if ((CO_CONFIG_USDO)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0
void
CO_USDOclient_initCallbackPre(CO_USDOclient_t* USDO_C, void* object, void (*pFunctSignalPre)(void* object)) {
    if (USDO_C != NULL) {
        USDO_C->functSignalObjectPre = object;
        USDO_C->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif

/* Write first three bytes of USDO request into transmit buffer */
static uint8_t*
USDO_C_header(CO_USDOclient_t* USDO_C, uint8_t cs) {
    uint8_t* data = USDO_C->CANtxBuff->data;
    data[0] = USDO_C->serverNodeId;
    data[1] = cs;
    data[2] = USDO_C->sessionId;
    return data;
}

/* Write header with sub-index and index (6 bytes) */
static uint8_t*
USDO_C_headerIdx(CO_USDOclient_t* USDO_C, uint8_t cs) {
    uint8_t* data = USDO_C_header(USDO_C, cs);
    data[3] = USDO_C->subIndex;
    data[4] = (uint8_t)USDO_C->index;
    data[5] = (uint8_t)(USDO_C->index >> 8);
    return data;
}

/* Send USDO request frame. Data must be already written into USDO_C->CANtxBuff->data. */
static void
USDO_C_send(CO_USDOclient_t* USDO_C, uint8_t length) {
    uint8_t frameLength = CO_CANframeLength(length);
    uint8_t* data = USDO_C->CANtxBuff->data;

    if (frameLength > length) {
        (void)memset((void*)&data[length], (int)CO_CAN_FD_PADDING_BYTE, (size_t)frameLength - length);
    }

    CO_CANtx_t* CANtxBuff = CO_CANtxBufferInit(USDO_C->CANdevTx, USDO_C->CANdevTxIdx,
                                               (uint16_t)CO_CAN_ID_USDO_REQ + USDO_C->nodeId, false, frameLength,
                                               false);
    if (CANtxBuff != NULL) {
        USDO_C->CANtxBuff = CANtxBuff;
        (void)CO_CANsend(USDO_C->CANdevTx, CANtxBuff);
    }
    USDO_C->timeoutTimer = 0;
}

/* Send next download segment or end segment */
static void
USDO_C_sendDownloadSegment(CO_USDOclient_t* USDO_C) {
    size_t remain = USDO_C->bufSize - USDO_C->sizeTran;
    uint8_t* data;

    if (remain <= CO_USDO_SEGMENT_SIZE) {
        data = USDO_C_header(USDO_C, (uint8_t)CO_USDO_CS_DL_SEG_END_REQ);
        data[3] = (uint8_t)remain;
        (void)memcpy((void*)&data[4], (const void*)&USDO_C->dataDownload[USDO_C->sizeTran], remain);
        USDO_C->sizeTran += remain;
        USDO_C->state = CO_USDO_CLI_ST_DL_END;
        USDO_C_send(USDO_C, (uint8_t)(4U + remain));
    } else {
        data = USDO_C_header(USDO_C, (uint8_t)CO_USDO_CS_DL_SEG_REQ);
        data[3] = USDO_C->segCounter;
        (void)memcpy((void*)&data[4], (const void*)&USDO_C->dataDownload[USDO_C->sizeTran], CO_USDO_SEGMENT_SIZE);
        USDO_C->sizeTran += CO_USDO_SEGMENT_SIZE;
        USDO_C->state = CO_USDO_CLI_ST_DL_SEG;
        USDO_C_send(USDO_C, CO_USDO_SEGMENT_LEN);
    }
}

/* Start new transfer, common part */
static CO_USDO_return_t
USDO_C_start(CO_USDOclient_t* USDO_C, uint8_t serverNodeId, uint16_t index, uint8_t subIndex, uint16_t timeoutTime_ms) {
    if (USDO_C->state != CO_USDO_CLI_ST_IDLE) {
        return CO_USDO_RT_busy;
    }
    if (USDO_C->CANtxBuff->bufferFull) {
        return CO_USDO_RT_busy;
    }
    USDO_C->serverNodeId = serverNodeId;
    USDO_C->index = index;
    USDO_C->subIndex = subIndex;
    USDO_C->sessionId++;
    if (USDO_C->sessionId == 0U) {
        USDO_C->sessionId = 1;
    }
    USDO_C->segCounter = 0;
    USDO_C->sizeTran = 0;
    USDO_C->sizeInd = 0;
    USDO_C->abortCode = CO_USDO_AB_NONE;
    USDO_C->timeoutTime_us = (uint32_t)timeoutTime_ms * 1000U;
    USDO_C->timeoutTimer = 0;
    CO_FLAG_CLEAR(USDO_C->CANrxNew);
    return CO_USDO_RT_waitingResponse;
}

CO_USDO_return_t
CO_USDOclient_download(CO_USDOclient_t* USDO_C, uint8_t serverNodeId, uint16_t index, uint8_t subIndex,
                       uint8_t dataType, const void* data, size_t size, uint16_t timeoutTime_ms) {
    if ((USDO_C == NULL) || (serverNodeId < 1U) || (serverNodeId > 127U) || ((data == NULL) && (size > 0U))) {
        return CO_USDO_RT_wrongArguments;
    }
    CO_USDO_return_t ret = USDO_C_start(USDO_C, serverNodeId, index, subIndex, timeoutTime_ms);
    if (ret != CO_USDO_RT_waitingResponse) {
        return ret;
    }

    USDO_C->dataDownload = (const uint8_t*)data;
    USDO_C->dataUpload = NULL;
    USDO_C->bufSize = size;
    USDO_C->dataType = dataType;

    if (size <= CO_USDO_EXPEDITED_MAX) {
        uint8_t* dataTx = USDO_C_headerIdx(USDO_C, (uint8_t)CO_USDO_CS_DL_EXP_REQ);
        dataTx[6] = dataType;
        dataTx[7] = (uint8_t)size;
        if (size > 0U) {
            (void)memcpy((void*)&dataTx[8], data, size);
        }
        USDO_C->sizeTran = size;
        USDO_C->state = CO_USDO_CLI_ST_DL_EXP;
        USDO_C_send(USDO_C, (uint8_t)(8U + size));
    } else {
        uint8_t* dataTx = USDO_C_headerIdx(USDO_C, (uint8_t)CO_USDO_CS_DL_SEG_INIT_REQ);
        dataTx[6] = dataType;
        dataTx[7] = 0;
        (void)CO_setUint32(&dataTx[8], CO_SWAP_32((uint32_t)size));
        USDO_C->state = CO_USDO_CLI_ST_DL_INIT;
        USDO_C_send(USDO_C, 12);
    }
    return CO_USDO_RT_waitingResponse;
}

CO_USDO_return_t
CO_USDOclient_upload(CO_USDOclient_t* USDO_C, uint8_t serverNodeId, uint16_t index, uint8_t subIndex, void* buf,
                     size_t bufSize, uint16_t timeoutTime_ms) {
    if ((USDO_C == NULL) || (serverNodeId < 1U) || (serverNodeId > 127U) || (buf == NULL)) {
        return CO_USDO_RT_wrongArguments;
    }
    CO_USDO_return_t ret = USDO_C_start(USDO_C, serverNodeId, index, subIndex, timeoutTime_ms);
    if (ret != CO_USDO_RT_waitingResponse) {
        return ret;
    }

    USDO_C->dataDownload = NULL;
    USDO_C->dataUpload = (uint8_t*)buf;
    USDO_C->bufSize = bufSize;
    USDO_C->dataType = (uint8_t)CO_USDO_DT_UNKNOWN;

    (void)USDO_C_headerIdx(USDO_C, (uint8_t)CO_USDO_CS_UL_REQ);
    USDO_C->state = CO_USDO_CLI_ST_UL_INIT;
    USDO_C_send(USDO_C, 6);
    return CO_USDO_RT_waitingResponse;
}

void
CO_USDOclient_abort(CO_USDOclient_t* USDO_C, CO_USDO_abortCode_t abortCode) {
    if ((USDO_C == NULL) || (USDO_C->state == CO_USDO_CLI_ST_IDLE)) {
        return;
    }
    uint8_t* data = USDO_C_headerIdx(USDO_C, (uint8_t)CO_USDO_CS_ABORT);
    data[6] = (uint8_t)abortCode;
    USDO_C_send(USDO_C, 7);
    USDO_C->abortCode = abortCode;
    USDO_C->state = CO_USDO_CLI_ST_IDLE;
    CO_FLAG_CLEAR(USDO_C->CANrxNew);
}

/* Copy uploaded data into the user buffer. Returns false, if buffer is too small. */
static bool_t
USDO_C_uploadData(CO_USDOclient_t* USDO_C, const uint8_t* data, size_t count) {
    if ((USDO_C->sizeTran + count) > USDO_C->bufSize) {
        return false;
    }
    (void)memcpy((void*)&USDO_C->dataUpload[USDO_C->sizeTran], (const void*)data, count);
    USDO_C->sizeTran += count;
    return true;
}

/*
 * Process response from the server.
 *
 * Returns abort code (client abort), if response is not correct, CO_USDO_AB_NONE otherwise.
 */
static CO_USDO_abortCode_t
USDO_C_processResponse(CO_USDOclient_t* USDO_C, const uint8_t* data, uint8_t len) {
    uint8_t cs = data[1];

    switch (USDO_C->state) {
        case CO_USDO_CLI_ST_DL_EXP:
            if (cs != (uint8_t)CO_USDO_CS_DL_EXP_RSP) {
                return CO_USDO_AB_CMD;
            }
            USDO_C->state = CO_USDO_CLI_ST_IDLE;
            break;

        case CO_USDO_CLI_ST_DL_INIT:
            if (cs != (uint8_t)CO_USDO_CS_DL_SEG_INIT_RSP) {
                return CO_USDO_AB_CMD;
            }
            USDO_C_sendDownloadSegment(USDO_C);
            break;

        case CO_USDO_CLI_ST_DL_SEG:
            if (cs != (uint8_t)CO_USDO_CS_DL_SEG_RSP) {
                return CO_USDO_AB_CMD;
            }
            if ((len < 4U) || (data[3] != USDO_C->segCounter)) {
                return CO_USDO_AB_SEG_COUNTER;
            }
            USDO_C->segCounter++;
            USDO_C_sendDownloadSegment(USDO_C);
            break;

        case CO_USDO_CLI_ST_DL_END:
            if (cs != (uint8_t)CO_USDO_CS_DL_SEG_END_RSP) {
                return CO_USDO_AB_CMD;
            }
            USDO_C->state = CO_USDO_CLI_ST_IDLE;
            break;

        case CO_USDO_CLI_ST_UL_INIT:
            if (cs == (uint8_t)CO_USDO_CS_UL_EXP_RSP) {
                uint8_t count = (len >= 8U) ? data[7] : 0U;
                if ((len < 8U) || (count > CO_USDO_EXPEDITED_MAX) || (len < (8U + count))) {
                    return CO_USDO_AB_DLC;
                }
                USDO_C->dataType = data[6];
                if (!USDO_C_uploadData(USDO_C, &data[8], count)) {
                    return CO_USDO_AB_OUT_OF_MEM;
                }
                USDO_C->state = CO_USDO_CLI_ST_IDLE;
            } else if (cs == (uint8_t)CO_USDO_CS_UL_SEG_INIT_RSP) {
                if (len < 12U) {
                    return CO_USDO_AB_DLC;
                }
                USDO_C->dataType = data[6];
                USDO_C->sizeInd = (size_t)CO_SWAP_32(CO_getUint32(&data[8]));
                if (USDO_C->sizeInd > USDO_C->bufSize) {
                    return CO_USDO_AB_OUT_OF_MEM;
                }
                uint8_t* dataTx = USDO_C_header(USDO_C, (uint8_t)CO_USDO_CS_UL_SEG_REQ);
                dataTx[3] = USDO_C->segCounter;
                USDO_C->state = CO_USDO_CLI_ST_UL_SEG;
                USDO_C_send(USDO_C, 4);
            } else {
                return CO_USDO_AB_CMD;
            }
            break;

        case CO_USDO_CLI_ST_UL_SEG:
            if (cs == (uint8_t)CO_USDO_CS_UL_SEG_RSP) {
                if ((len < CO_USDO_SEGMENT_LEN) || (data[3] != USDO_C->segCounter)) {
                    return (len < CO_USDO_SEGMENT_LEN) ? CO_USDO_AB_DLC : CO_USDO_AB_SEG_COUNTER;
                }
                if (!USDO_C_uploadData(USDO_C, &data[4], CO_USDO_SEGMENT_SIZE)) {
                    return CO_USDO_AB_OUT_OF_MEM;
                }
                USDO_C->segCounter++;
                uint8_t* dataTx = USDO_C_header(USDO_C, (uint8_t)CO_USDO_CS_UL_SEG_REQ);
                dataTx[3] = USDO_C->segCounter;
                USDO_C_send(USDO_C, 4);
            } else if (cs == (uint8_t)CO_USDO_CS_UL_SEG_END_RSP) {
                uint8_t count = (len >= 4U) ? data[3] : 0U;
                if ((len < 4U) || (count > CO_USDO_SEGMENT_SIZE) || (len < (4U + count))) {
                    return CO_USDO_AB_DLC;
                }
                if (!USDO_C_uploadData(USDO_C, &data[4], count)) {
                    return CO_USDO_AB_OUT_OF_MEM;
                }
                if ((USDO_C->sizeInd > 0U) && (USDO_C->sizeInd != USDO_C->sizeTran)) {
                    return CO_USDO_AB_DATA_SIZE;
                }
                USDO_C->state = CO_USDO_CLI_ST_IDLE;
            } else {
                return CO_USDO_AB_CMD;
            }
            break;

        default: break;
    }
    return CO_USDO_AB_NONE;
}

CO_USDO_return_t
CO_USDOclient_process(CO_USDOclient_t* USDO_C, uint32_t timeDifference_us, uint32_t* timerNext_us,
                      CO_USDO_abortCode_t* abortCode, size_t* sizeTransferred) {
    CO_USDO_return_t ret = CO_USDO_RT_waitingResponse;
    (void)timerNext_us; /* may be unused */

    if (USDO_C == NULL) {
        return CO_USDO_RT_wrongArguments;
    }

    if (USDO_C->state == CO_USDO_CLI_ST_IDLE) {
        ret = CO_USDO_RT_ok_communicationEnd;
    } else if (CO_FLAG_READ(USDO_C->CANrxNew)) {
        const uint8_t* data = USDO_C->CANrxData;
        uint8_t len = USDO_C->CANrxLen;

        if (data[1] == (uint8_t)CO_USDO_CS_ABORT) {
            /* abort from server */
            USDO_C->abortCode = (len >= 7U) ? (CO_USDO_abortCode_t)data[6] : CO_USDO_AB_GENERAL;
            USDO_C->state = CO_USDO_CLI_ST_IDLE;
            CO_FLAG_CLEAR(USDO_C->CANrxNew);
            ret = CO_USDO_RT_endedWithServerAbort;
        } else if (USDO_C->CANtxBuff->bufferFull) {
            /* wait, response will be processed later */
        } else {
            CO_USDO_abortCode_t ab = USDO_C_processResponse(USDO_C, data, len);
            CO_FLAG_CLEAR(USDO_C->CANrxNew);
            if (ab != CO_USDO_AB_NONE) {
                CO_USDOclient_abort(USDO_C, ab);
                ret = CO_USDO_RT_endedWithClientAbort;
            } else if (USDO_C->state == CO_USDO_CLI_ST_IDLE) {
                ret = CO_USDO_RT_ok_communicationEnd;
            } else { /* MISRA C 2004 14.10 */
            }
        }
    } else {
        /* timeout */
        if (USDO_C->timeoutTimer < USDO_C->timeoutTime_us) {
            USDO_C->timeoutTimer += timeDifference_us;
        }
        if (USDO_C->timeoutTimer >= USDO_C->timeoutTime_us) {
            CO_USDOclient_abort(USDO_C, CO_USDO_AB_TIMEOUT);
            ret = CO_USDO_RT_endedWithClientAbort;
        }
#if ((CO_CONFIG_USDO)&CO_CONFIG_FLAG_TIMERNEXT) != 0
        else if (timerNext_us != NULL) {
            uint32_t diff = USDO_C->timeoutTime_us - USDO_C->timeoutTimer;
            if (*timerNext_us > diff) {
                *timerNext_us = diff;
            }
        } else { /* MISRA C 2004 14.10 */
        }
#endif
    }

    if (abortCode != NULL) {
        *abortCode = USDO_C->abortCode;
    }
    if (sizeTransferred != NULL) {
        *sizeTransferred = USDO_C->sizeTran;
    }
    return ret;
}

#endif /* (CO_CONFIG_USDO) & CO_CONFIG_USDO_CLI_ENABLE */
