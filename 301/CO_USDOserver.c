/*
 * CANopen FD Universal Service Data Object - server protocol.
 *
 * @file        CO_USDOserver.c
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

#include <string.h>

#include "301/CO_USDOserver.h"

#if ((CO_CONFIG_USDO)&(CO_CONFIG_USDO_SRV_ENABLE | CO_CONFIG_USDO_CLI_ENABLE)) != 0

CO_USDO_abortCode_t
CO_USDO_abortCodeFromODR(ODR_t returnCode) {
    static const uint8_t abortCodes[ODR_COUNT] = {
        (uint8_t)CO_USDO_AB_NONE,           /* ODR_OK */
        (uint8_t)CO_USDO_AB_OUT_OF_MEM,     /* ODR_OUT_OF_MEM */
        (uint8_t)CO_USDO_AB_UNSUPP_ACCESS,  /* ODR_UNSUPP_ACCESS */
        (uint8_t)CO_USDO_AB_WRITEONLY,      /* ODR_WRITEONLY */
        (uint8_t)CO_USDO_AB_READONLY,       /* ODR_READONLY */
        (uint8_t)CO_USDO_AB_NOT_EXIST,      /* ODR_IDX_NOT_EXIST */
        (uint8_t)CO_USDO_AB_NO_MAP,         /* ODR_NO_MAP */
        (uint8_t)CO_USDO_AB_MAP_LEN,        /* ODR_MAP_LEN */
        (uint8_t)CO_USDO_AB_INVALID_VALUE,  /* ODR_PAR_INCOMPAT */
        (uint8_t)CO_USDO_AB_GENERAL,        /* ODR_DEV_INCOMPAT */
        (uint8_t)CO_USDO_AB_HW,             /* ODR_HW */
        (uint8_t)CO_USDO_AB_TYPE_MISMATCH,  /* ODR_TYPE_MISMATCH */
        (uint8_t)CO_USDO_AB_DATA_LONG,      /* ODR_DATA_LONG */
        (uint8_t)CO_USDO_AB_DATA_SHORT,     /* ODR_DATA_SHORT */
        (uint8_t)CO_USDO_AB_SUB_UNKNOWN,    /* ODR_SUB_NOT_EXIST */
        (uint8_t)CO_USDO_AB_INVALID_VALUE,  /* ODR_INVALID_VALUE */
        (uint8_t)CO_USDO_AB_VALUE_HIGH,     /* ODR_VALUE_HIGH */
        (uint8_t)CO_USDO_AB_VALUE_LOW,      /* ODR_VALUE_LOW */
        (uint8_t)CO_USDO_AB_MAX_LESS_MIN,   /* ODR_MAX_LESS_MIN */
        (uint8_t)CO_USDO_AB_OUT_OF_MEM,     /* ODR_NO_RESOURCE */
        (uint8_t)CO_USDO_AB_GENERAL,        /* ODR_GENERAL */
        (uint8_t)CO_USDO_AB_DATA_TRANSF,    /* ODR_DATA_TRANSF */
        (uint8_t)CO_USDO_AB_DATA_LOC_CTRL,  /* ODR_DATA_LOC_CTRL */
        (uint8_t)CO_USDO_AB_DATA_DEV_STATE, /* ODR_DATA_DEV_STATE */
        (uint8_t)CO_USDO_AB_DATA_OD,        /* ODR_OD_MISSING */
        (uint8_t)CO_USDO_AB_NO_DATA         /* ODR_NO_DATA */
    };

    return ((returnCode >= ODR_OK) && (returnCode < ODR_COUNT)) ? (CO_USDO_abortCode_t)abortCodes[returnCode]
                                                                : CO_USDO_AB_GENERAL;
}

#endif

#if ((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_ENABLE) != 0

/*
 * Read received frame from CAN module
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN frame with correct identifier will be
 * received. For more information and description of parameters see file CO_driver.h.
 */
static void
CO_USDOserver_receive(void* object, void* msg) {
    CO_USDOserver_t* USDO = (CO_USDOserver_t*)object;
    uint8_t clientNodeId = (uint8_t)(CO_CANrxMsg_readIdent(msg) & 0x7FU);
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    const uint8_t* data = CO_CANrxMsg_readData(msg);

    /* frame must be addressed to this node or broadcast */
    if ((clientNodeId == 0U) || (DLC < 3U) || ((data[0] != USDO->nodeId) && (data[0] != 0U))) {
        return;
    }

    uint8_t slot = USDO->CANrxWr;
    if (CO_FLAG_READ(USDO->CANrxNew[slot])) {
        /* fifo is full, frame is lost */
        USDO->CANrxOverflow = true;
        return;
    }

    if (DLC > 64U) {
        DLC = 64U;
    }
    (void)memcpy((void*)USDO->CANrxData[slot], (const void*)data, DLC);
    USDO->CANrxLen[slot] = DLC;
    USDO->CANrxClient[slot] = clientNodeId;
    CO_FLAG_SET(USDO->CANrxNew[slot]);
    slot++;
    USDO->CANrxWr = (slot < (uint8_t)CO_CONFIG_USDO_SRV_RX_FIFO) ? slot : 0U;

#if ((CO_CONFIG_USDO)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    /* Optional signal to RTOS, which can resume task, which handles USDO server. */
    if (USDO->pFunctSignalPre != NULL) {
        USDO->pFunctSignalPre(USDO->functSignalObjectPre);
    }
#endif
}

CO_ReturnError_t
CO_USDOserver_init(CO_USDOserver_t* USDO, OD_t* OD, uint8_t nodeId, uint16_t timeoutTime_ms, CO_CANmodule_t* CANdevRx,
                   uint16_t CANdevRxIdx, CO_CANmodule_t* CANdevTx, uint16_t CANdevTxIdx, uint32_t* errInfo) {
    (void)errInfo;

    /* verify arguments */
    if ((USDO == NULL) || (OD == NULL) || (CANdevRx == NULL) || (CANdevTx == NULL) || (nodeId < 1U)
        || (nodeId > 127U)) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    (void)memset((void*)USDO, 0, sizeof(CO_USDOserver_t));

    USDO->OD = OD;
    USDO->nodeId = nodeId;
    USDO->state = CO_USDO_ST_IDLE;
    USDO->timeoutTime_us = (uint32_t)timeoutTime_ms * 1000U;
    USDO->CANdevTx = CANdevTx;
    USDO->CANdevTxIdx = CANdevTxIdx;

    /* receive requests from all clients: CAN-ID 0x600 to 0x67F */
    CO_ReturnError_t ret = CO_CANrxBufferInit(CANdevRx, CANdevRxIdx, (uint16_t)CO_CAN_ID_USDO_REQ, 0x780U, false,
                                              (void*)USDO, CO_USDOserver_receive);
    if (ret != CO_ERROR_NO) {
        return ret;
    }

    USDO->CANtxBuff = CO_CANtxBufferInit(CANdevTx, CANdevTxIdx, (uint16_t)CO_CAN_ID_USDO_RSP + nodeId, false,
                                         CO_USDO_SEGMENT_LEN, false);
    if (USDO->CANtxBuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return CO_ERROR_NO;
}

#if ((CO_CONFIG_USDO)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0
void
CO_USDOserver_initCallbackPre(CO_USDOserver_t* USDO, void* object, void (*pFunctSignalPre)(void* object)) {
    if (USDO != NULL) {
        USDO->functSignalObjectPre = object;
        USDO->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif

void
CO_USDOserver_initCallbackDataType(CO_USDOserver_t* USDO, void* object,
                                   uint8_t (*pFunctGetDataType)(void* object, uint16_t index, uint8_t subIndex,
                                                                const OD_stream_t* stream)) {
    if (USDO != NULL) {
        USDO->functGetDataTypeObject = object;
        USDO->pFunctGetDataType = pFunctGetDataType;
    }
}

/* Send USDO response frame. Data must be already written into USDO->CANtxBuff->data. */
static void
USDO_send(CO_USDOserver_t* USDO, uint8_t length) {
    uint8_t frameLength = CO_CANframeLength(length);
    uint8_t* data = USDO->CANtxBuff->data;

    /* padding */
    if (frameLength > length) {
        (void)memset((void*)&data[length], (int)CO_CAN_FD_PADDING_BYTE, (size_t)frameLength - length);
    }

    /* Length of USDO response varies, configure it in the transmit buffer. Data are preserved. */
    CO_CANtx_t* CANtxBuff = CO_CANtxBufferInit(USDO->CANdevTx, USDO->CANdevTxIdx,
                                               (uint16_t)CO_CAN_ID_USDO_RSP + USDO->nodeId, false, frameLength, false);
    if (CANtxBuff != NULL) {
        USDO->CANtxBuff = CANtxBuff;
        (void)CO_CANsend(USDO->CANdevTx, CANtxBuff);
    }
}

/* Write first three bytes of USDO response into transmit buffer */
static uint8_t*
USDO_header(CO_USDOserver_t* USDO, uint8_t clientNodeId, uint8_t cs, uint8_t sessionId) {
    uint8_t* data = USDO->CANtxBuff->data;
    data[0] = clientNodeId;
    data[1] = cs;
    data[2] = sessionId;
    return data;
}

/* Write header with sub-index and index (6 bytes) */
static uint8_t*
USDO_headerIdx(CO_USDOserver_t* USDO, uint8_t cs) {
    uint8_t* data = USDO_header(USDO, USDO->clientNodeId, cs, USDO->sessionId);
    data[3] = USDO->subIndex;
    data[4] = (uint8_t)USDO->index;
    data[5] = (uint8_t)(USDO->index >> 8);
    return data;
}

/* Send abort to client and reset state */
static void
USDO_abort(CO_USDOserver_t* USDO, uint8_t clientNodeId, uint8_t sessionId, uint16_t index, uint8_t subIndex,
           CO_USDO_abortCode_t abortCode) {
    uint8_t* data = USDO_header(USDO, clientNodeId, (uint8_t)CO_USDO_CS_ABORT, sessionId);
    data[3] = subIndex;
    data[4] = (uint8_t)index;
    data[5] = (uint8_t)(index >> 8);
    data[6] = (uint8_t)abortCode;
    USDO_send(USDO, 7);
}

/* Abort current transfer */
static void
USDO_abortCurrent(CO_USDOserver_t* USDO, CO_USDO_abortCode_t abortCode) {
    USDO_abort(USDO, USDO->clientNodeId, USDO->sessionId, USDO->index, USDO->subIndex, abortCode);
    USDO->state = CO_USDO_ST_IDLE;
}

#ifdef CO_BIG_ENDIAN
static void
reverseBytes(void* start, OD_size_t size) {
    uint8_t* lo = (uint8_t*)start;
    uint8_t* hi = (uint8_t*)start + size - 1;
    uint8_t swap;
    while (lo < hi) {
        swap = *lo;
        *lo++ = *hi;
        *hi-- = swap;
    }
}
#endif

/* Determine data type of the current OD variable */
static uint8_t
USDO_dataType(CO_USDOserver_t* USDO) {
    const OD_stream_t* stream = &USDO->OD_IO.stream;
    uint8_t dataType = (uint8_t)CO_USDO_DT_UNKNOWN;

    if (USDO->pFunctGetDataType != NULL) {
        dataType = USDO->pFunctGetDataType(USDO->functGetDataTypeObject, USDO->index, USDO->subIndex, stream);
    }
    if (dataType == (uint8_t)CO_USDO_DT_UNKNOWN) {
        if ((stream->attribute & (OD_attr_t)ODA_STR) != 0U) {
            dataType = (uint8_t)CO_USDO_DT_VISIBLE_STRING;
        } else if (stream->dataLength == 0U) {
            dataType = (uint8_t)CO_USDO_DT_DOMAIN;
        } else if (stream->dataLength == 1U) {
            dataType = (uint8_t)CO_USDO_DT_UNSIGNED8;
        } else if ((stream->attribute & (OD_attr_t)ODA_MB) != 0U) {
            switch (stream->dataLength) {
                case 2: dataType = (uint8_t)CO_USDO_DT_UNSIGNED16; break;
                case 3: dataType = (uint8_t)CO_USDO_DT_UNSIGNED24; break;
                case 4: dataType = (uint8_t)CO_USDO_DT_UNSIGNED32; break;
                case 8: dataType = (uint8_t)CO_USDO_DT_UNSIGNED64; break;
                default: dataType = (uint8_t)CO_USDO_DT_OCTET_STRING; break;
            }
        } else {
            dataType = (uint8_t)CO_USDO_DT_OCTET_STRING;
        }
    }
    return dataType;
}

/*
 * Write data from buffer into OD variable. Function also verifies data size and swaps data if necessary.
 *
 * Similar as validateAndWriteToOD() in CO_SDOserver.c.
 *
 * Returns USDO abort code, CO_USDO_AB_NONE on success.
 */
static CO_USDO_abortCode_t
USDO_writeToOD(CO_USDOserver_t* USDO) {
    if (USDO->finished) {
        /* Verify if size of data downloaded matches size indicated. */
        if ((USDO->sizeInd > 0U) && (USDO->sizeTran != USDO->sizeInd)) {
            return (USDO->sizeTran > USDO->sizeInd) ? CO_USDO_AB_DATA_LONG : CO_USDO_AB_DATA_SHORT;
        }

#ifdef CO_BIG_ENDIAN
        /* swap int16_t .. uint64_t data if necessary */
        if ((USDO->OD_IO.stream.attribute & ODA_MB) != 0) {
            reverseBytes(USDO->buf, USDO->bufOffsetWr);
        }
#endif

        OD_size_t sizeInOd = USDO->OD_IO.stream.dataLength;

        /* If dataType is string, then size of data downloaded may be shorter than size of the OD data buffer. If so,
         * add two zero bytes to terminate (unicode) string. Shorten also OD data size, (temporary, send information
         * about EOF into OD_IO.write) */
        if (((USDO->OD_IO.stream.attribute & (OD_attr_t)ODA_STR) != 0U)
            && ((sizeInOd == 0U) || (USDO->sizeTran < sizeInOd))
            && ((USDO->bufOffsetWr + 2U) <= CO_CONFIG_USDO_SRV_BUFFER_SIZE)) {
            USDO->buf[USDO->bufOffsetWr] = 0;
            USDO->bufOffsetWr++;
            USDO->sizeTran++;
            if ((sizeInOd == 0U) || (USDO->sizeTran < sizeInOd)) {
                USDO->buf[USDO->bufOffsetWr] = 0;
                USDO->bufOffsetWr++;
                USDO->sizeTran++;
            }
            USDO->OD_IO.stream.dataLength = USDO->sizeTran;
        }
        /* Indicate OD data size, if not indicated. Can be used for EOF check. */
        else if (sizeInOd == 0U) {
            USDO->OD_IO.stream.dataLength = USDO->sizeTran;
        }
        /* Verify if size of data downloaded matches data size in OD. */
        else if (USDO->sizeTran != sizeInOd) {
            return (USDO->sizeTran > sizeInOd) ? CO_USDO_AB_DATA_LONG : CO_USDO_AB_DATA_SHORT;
        } else { /* MISRA C 2004 14.10 */
        }
    } else {
        /* Verify if size of data downloaded is not too large. */
        if ((USDO->sizeInd > 0U) && (USDO->sizeTran > USDO->sizeInd)) {
            return CO_USDO_AB_DATA_LONG;
        }
    }

    /* write data */
    OD_size_t countWritten = 0;
    ODR_t odRet;

    CO_LOCK_OD(USDO->CANdevTx);
    odRet = USDO->OD_IO.write(&USDO->OD_IO.stream, USDO->buf, USDO->bufOffsetWr, &countWritten);
    CO_UNLOCK_OD(USDO->CANdevTx);

    USDO->bufOffsetWr = 0;

    /* verify write error value */
    if ((odRet != ODR_OK) && (odRet != ODR_PARTIAL)) {
        return CO_USDO_abortCodeFromODR(odRet);
    } else if (USDO->finished && (odRet == ODR_PARTIAL)) {
        /* OD variable was not written completely, but USDO download finished */
        return CO_USDO_AB_DATA_SHORT;
    } else if (!USDO->finished && (odRet == ODR_OK)) {
        /* OD variable was written completely, but USDO download still has data */
        return CO_USDO_AB_DATA_LONG;
    } else { /* MISRA C 2004 14.10 */
    }

    return CO_USDO_AB_NONE;
}

/*
 * Read data from OD variable into the buffer, if there is less than countMinimum bytes in the buffer.
 *
 * Similar as readFromOd() in CO_SDOserver.c.
 *
 * Returns USDO abort code, CO_USDO_AB_NONE on success.
 */
static CO_USDO_abortCode_t
USDO_readFromOD(CO_USDOserver_t* USDO, OD_size_t countMinimum) {
    OD_size_t countRemain = USDO->bufOffsetWr - USDO->bufOffsetRd;

    if (!USDO->finished && (countRemain < countMinimum)) {
        /* first move remaining data to the start of the buffer */
        (void)memmove((void*)USDO->buf, (const void*)&USDO->buf[USDO->bufOffsetRd], countRemain);
        USDO->bufOffsetRd = 0;
        USDO->bufOffsetWr = countRemain;

        /* load data from OD variable into the buffer */
        OD_size_t countRdRequest = CO_CONFIG_USDO_SRV_BUFFER_SIZE - countRemain;
        OD_size_t countRd = 0;
        ODR_t odRet;

        CO_LOCK_OD(USDO->CANdevTx);
        odRet = USDO->OD_IO.read(&USDO->OD_IO.stream, &USDO->buf[countRemain], countRdRequest, &countRd);
        CO_UNLOCK_OD(USDO->CANdevTx);

        if ((odRet != ODR_OK) && (odRet != ODR_PARTIAL)) {
            return CO_USDO_abortCodeFromODR(odRet);
        }

        /* if data is string, send only data up to null termination */
        OD_size_t lastRd = countRd + countRemain;
        if ((countRd > 0U) && ((USDO->OD_IO.stream.attribute & (OD_attr_t)ODA_STR) != 0U)) {
            USDO->buf[lastRd] = 0; /* (USDO->buf is one byte larger) */
            OD_size_t countStr = (OD_size_t)strlen((char*)&USDO->buf[countRemain]);
            if (countStr == 0U) {
                countStr = 1; /* zero length is not allowed */
            }
            if (countStr < countRd) {
                /* string terminator found, read is finished, shorten data */
                countRd = countStr;
                odRet = ODR_OK;
                USDO->OD_IO.stream.dataLength = USDO->sizeTran + countRemain + countRd;
            }
        }

        /* partial or finished read */
        USDO->bufOffsetWr = countRemain + countRd;
        if ((USDO->bufOffsetWr == 0U) || (odRet == ODR_PARTIAL)) {
            USDO->finished = false;
            if (USDO->bufOffsetWr < countMinimum) {
                return CO_USDO_AB_GENERAL;
            }
        } else {
            USDO->finished = true;
        }

#ifdef CO_BIG_ENDIAN
        /* swap data if necessary */
        if ((USDO->OD_IO.stream.attribute & ODA_MB) != 0) {
            if (USDO->finished) {
                /* int16_t .. uint64_t */
                reverseBytes(&USDO->buf[countRemain], countRd);
            } else {
                return CO_USDO_AB_TYPE_MISMATCH;
            }
        }
#endif
    }
    return CO_USDO_AB_NONE;
}

/*
 * Open OD variable for new transfer and verify access.
 *
 * Returns USDO abort code, CO_USDO_AB_NONE on success.
 */
static CO_USDO_abortCode_t
USDO_openOD(CO_USDOserver_t* USDO, bool_t upload) {
    ODR_t odRet = OD_getSub(OD_find(USDO->OD, USDO->index), USDO->subIndex, &USDO->OD_IO, false);
    if (odRet != ODR_OK) {
        return CO_USDO_abortCodeFromODR(odRet);
    }

    OD_attr_t attr = USDO->OD_IO.stream.attribute;
    if ((attr & (OD_attr_t)ODA_SDO_RW) == 0U) {
        return CO_USDO_AB_UNSUPP_ACCESS;
    }
    if (upload && ((attr & (OD_attr_t)ODA_SDO_R) == 0U)) {
        return CO_USDO_AB_WRITEONLY;
    }
    if (!upload && ((attr & (OD_attr_t)ODA_SDO_W) == 0U)) {
        return CO_USDO_AB_READONLY;
    }

    USDO->sizeInd = 0;
    USDO->sizeTran = 0;
    USDO->finished = false;
    USDO->bufOffsetWr = 0;
    USDO->bufOffsetRd = 0;
    USDO->segCounter = 0;
    USDO->timeoutTimer = 0;
    return CO_USDO_AB_NONE;
}

#if ((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_SEGMENTED) != 0
/*
 * Verify size indicated by client in segmented or bulk download initiate.
 *
 * Returns USDO abort code, CO_USDO_AB_NONE on success.
 */
static CO_USDO_abortCode_t
USDO_verifySizeInd(CO_USDOserver_t* USDO) {
    OD_size_t sizeInOd = USDO->OD_IO.stream.dataLength;

    if ((USDO->sizeInd > 0U) && (sizeInOd > 0U)) {
        if (USDO->sizeInd > sizeInOd) {
            return CO_USDO_AB_DATA_LONG;
        }
        if ((USDO->sizeInd < sizeInOd) && ((USDO->OD_IO.stream.attribute & (OD_attr_t)ODA_STR) == 0U)) {
            return CO_USDO_AB_DATA_SHORT;
        }
    }
    return CO_USDO_AB_NONE;
}

/*
 * Add data from download segment into the buffer and write the buffer to OD, if necessary.
 *
 * Returns USDO abort code, CO_USDO_AB_NONE on success.
 */
static CO_USDO_abortCode_t
USDO_downloadSegment(CO_USDOserver_t* USDO, const uint8_t* data, uint8_t count) {
    (void)memcpy((void*)&USDO->buf[USDO->bufOffsetWr], (const void*)data, count);
    USDO->bufOffsetWr += count;
    USDO->sizeTran += count;

    /* Write data to OD, if download is finished or if there is not enough space for the next segment. Two bytes are
     * reserved for possible string terminators. */
    if (USDO->finished || ((CO_CONFIG_USDO_SRV_BUFFER_SIZE - USDO->bufOffsetWr) < (CO_USDO_SEGMENT_SIZE + 2U))) {
        return USDO_writeToOD(USDO);
    }
    /* verify size */
    if ((USDO->sizeInd > 0U) && (USDO->sizeTran > USDO->sizeInd)) {
        return CO_USDO_AB_DATA_LONG;
    }
    return CO_USDO_AB_NONE;
}
#endif

/*
 * Process request, which starts new transfer: expedited download, segmented or bulk download initiate or upload.
 */
static void
USDO_processInitiate(CO_USDOserver_t* USDO, const uint8_t* data, uint8_t len, uint8_t cs) {
    bool_t broadcast = data[0] == 0U;
    bool_t upload = cs == (uint8_t)CO_USDO_CS_UL_REQ;
    CO_USDO_abortCode_t abortCode;

    USDO->subIndex = data[3];
    USDO->index = (uint16_t)(((uint16_t)data[5] << 8) | data[4]);

    if (broadcast && (cs != (uint8_t)CO_USDO_CS_DL_EXP_REQ) && (cs != (uint8_t)CO_USDO_CS_UL_REQ)
        && (cs != (uint8_t)CO_USDO_CS_DL_BULK_INIT_REQ)) {
        /* segmented transfer is not allowed with broadcast */
        USDO_abortCurrent(USDO, CO_USDO_AB_PROTOCOL);
        return;
    }

    abortCode = USDO_openOD(USDO, upload);
    if (abortCode != CO_USDO_AB_NONE) {
        USDO_abortCurrent(USDO, abortCode);
        return;
    }

    switch (cs) {
        case CO_USDO_CS_DL_EXP_REQ: {
            uint8_t count = (len >= 8U) ? data[7] : 0U;
            if ((len < 8U) || (count > CO_USDO_EXPEDITED_MAX) || (len < (8U + count))) {
                USDO_abortCurrent(USDO, CO_USDO_AB_DLC);
                return;
            }
            (void)memcpy((void*)USDO->buf, (const void*)&data[8], count);
            USDO->bufOffsetWr = count;
            USDO->sizeTran = count;
            USDO->sizeInd = count;
            USDO->finished = true;
            abortCode = USDO_writeToOD(USDO);
            if (abortCode != CO_USDO_AB_NONE) {
                USDO_abortCurrent(USDO, abortCode);
                return;
            }
            (void)USDO_headerIdx(USDO, (uint8_t)CO_USDO_CS_DL_EXP_RSP);
            USDO_send(USDO, 6);
            USDO->state = CO_USDO_ST_IDLE;
            break;
        }

        case CO_USDO_CS_UL_REQ: {
            abortCode = USDO_readFromOD(USDO, CO_USDO_EXPEDITED_MAX + 1U);
            if (abortCode != CO_USDO_AB_NONE) {
                USDO_abortCurrent(USDO, abortCode);
                return;
            }
            uint8_t dataType = USDO_dataType(USDO);
            OD_size_t count = USDO->bufOffsetWr - USDO->bufOffsetRd;

            if (USDO->finished && (count <= CO_USDO_EXPEDITED_MAX)) {
                /* expedited upload */
                uint8_t* dataTx = USDO_headerIdx(USDO, (uint8_t)CO_USDO_CS_UL_EXP_RSP);
                dataTx[6] = dataType;
                dataTx[7] = (uint8_t)count;
                (void)memcpy((void*)&dataTx[8], (const void*)&USDO->buf[USDO->bufOffsetRd], count);
                USDO_send(USDO, (uint8_t)(8U + count));
                USDO->state = CO_USDO_ST_IDLE;
            }
#if ((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_SEGMENTED) != 0
            else if (!broadcast) {
                /* segmented upload initiate, indicate size, if known */
                if (USDO->finished) {
                    USDO->sizeInd = count;
                } else if ((USDO->OD_IO.stream.attribute & (OD_attr_t)ODA_STR) == 0U) {
                    USDO->sizeInd = USDO->OD_IO.stream.dataLength;
                } else {
                    USDO->sizeInd = 0;
                }
                uint8_t* dataTx = USDO_headerIdx(USDO, (uint8_t)CO_USDO_CS_UL_SEG_INIT_RSP);
                dataTx[6] = dataType;
                dataTx[7] = 0;
                (void)CO_setUint32(&dataTx[8], CO_SWAP_32((uint32_t)USDO->sizeInd));
                USDO_send(USDO, 12);
                USDO->state = CO_USDO_ST_UL_SEG;
            }
#endif
            else {
                USDO_abortCurrent(USDO, CO_USDO_AB_OUT_OF_MEM);
            }
            break;
        }

#if ((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_SEGMENTED) != 0
        case CO_USDO_CS_DL_SEG_INIT_REQ:
#if ((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_BULK) != 0
        case CO_USDO_CS_DL_BULK_INIT_REQ:
#endif
        {
            if (len < 12U) {
                USDO_abortCurrent(USDO, CO_USDO_AB_DLC);
                return;
            }
            USDO->sizeInd = (OD_size_t)CO_SWAP_32(CO_getUint32(&data[8]));
            abortCode = USDO_verifySizeInd(USDO);
            if (abortCode != CO_USDO_AB_NONE) {
                USDO_abortCurrent(USDO, abortCode);
                return;
            }
            if (cs == (uint8_t)CO_USDO_CS_DL_SEG_INIT_REQ) {
                (void)USDO_headerIdx(USDO, (uint8_t)CO_USDO_CS_DL_SEG_INIT_RSP);
                USDO_send(USDO, 6);
                USDO->state = CO_USDO_ST_DL_SEG;
            } else {
                uint8_t* dataTx = USDO_headerIdx(USDO, (uint8_t)CO_USDO_CS_DL_BULK_INIT_RSP);
                (void)CO_setUint16(&dataTx[6], CO_SWAP_16((uint16_t)CO_CONFIG_USDO_SRV_BULK_TIME));
                USDO_send(USDO, 8);
                USDO->state = CO_USDO_ST_DL_BULK;
            }
            break;
        }
#endif

        default: USDO_abortCurrent(USDO, CO_USDO_AB_CMD); break;
    }
}

#if ((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_SEGMENTED) != 0
/*
 * Process segment of the current transfer.
 */
static void
USDO_processSegment(CO_USDOserver_t* USDO, const uint8_t* data, uint8_t len, uint8_t cs) {
    CO_USDO_abortCode_t abortCode = CO_USDO_AB_NONE;
    bool_t isEnd = (cs == (uint8_t)CO_USDO_CS_DL_SEG_END_REQ) || (cs == (uint8_t)CO_USDO_CS_DL_BULK_END_REQ);

    USDO->timeoutTimer = 0;

    switch (USDO->state) {
        case CO_USDO_ST_DL_SEG:
        case CO_USDO_ST_DL_BULK: {
            bool_t bulk = USDO->state == CO_USDO_ST_DL_BULK;
            uint8_t csSeg = bulk ? (uint8_t)CO_USDO_CS_DL_BULK_SEG : (uint8_t)CO_USDO_CS_DL_SEG_REQ;
            uint8_t csEnd = bulk ? (uint8_t)CO_USDO_CS_DL_BULK_END_REQ : (uint8_t)CO_USDO_CS_DL_SEG_END_REQ;

            if ((cs != csSeg) && (cs != csEnd)) {
                abortCode = CO_USDO_AB_CMD;
                break;
            }
            if (len < 4U) {
                abortCode = CO_USDO_AB_DLC;
                break;
            }

            if (isEnd) {
                uint8_t count = data[3];
                if ((count > CO_USDO_SEGMENT_SIZE) || (len < (4U + count))) {
                    abortCode = CO_USDO_AB_DLC;
                    break;
                }
                USDO->finished = true;
                abortCode = USDO_downloadSegment(USDO, &data[4], count);
                if (abortCode == CO_USDO_AB_NONE) {
                    (void)USDO_header(USDO, USDO->clientNodeId,
                                      bulk ? (uint8_t)CO_USDO_CS_DL_BULK_END_RSP : (uint8_t)CO_USDO_CS_DL_SEG_END_RSP,
                                      USDO->sessionId);
                    USDO_send(USDO, 3);
                    USDO->state = CO_USDO_ST_IDLE;
                }
            } else {
                uint8_t counter = data[3];
                if (counter != USDO->segCounter) {
                    abortCode = CO_USDO_AB_SEG_COUNTER;
                    break;
                }
                if (len < CO_USDO_SEGMENT_LEN) {
                    abortCode = CO_USDO_AB_DLC;
                    break;
                }
                abortCode = USDO_downloadSegment(USDO, &data[4], CO_USDO_SEGMENT_SIZE);
                if (abortCode == CO_USDO_AB_NONE) {
                    USDO->segCounter++;
                    if (!bulk) {
                        uint8_t* dataTx = USDO_header(USDO, USDO->clientNodeId, (uint8_t)CO_USDO_CS_DL_SEG_RSP,
                                                      USDO->sessionId);
                        dataTx[3] = counter;
                        USDO_send(USDO, 4);
                    }
                }
            }
            break;
        }

        case CO_USDO_ST_UL_SEG: {
            if (cs != (uint8_t)CO_USDO_CS_UL_SEG_REQ) {
                abortCode = CO_USDO_AB_CMD;
                break;
            }
            if (len < 4U) {
                abortCode = CO_USDO_AB_DLC;
                break;
            }
            uint8_t counter = data[3];
            if (counter != USDO->segCounter) {
                abortCode = CO_USDO_AB_SEG_COUNTER;
                break;
            }

            abortCode = USDO_readFromOD(USDO, CO_USDO_SEGMENT_SIZE);
            if (abortCode != CO_USDO_AB_NONE) {
                break;
            }

            OD_size_t count = USDO->bufOffsetWr - USDO->bufOffsetRd;
            bool_t lastSegment = USDO->finished && (count <= CO_USDO_SEGMENT_SIZE);
            if (count > CO_USDO_SEGMENT_SIZE) {
                count = CO_USDO_SEGMENT_SIZE;
            }
            USDO->sizeTran += count;

            /* verify size of data */
            if ((USDO->sizeInd > 0U)
                && ((USDO->sizeTran > USDO->sizeInd) || (lastSegment && (USDO->sizeTran < USDO->sizeInd)))) {
                abortCode = (USDO->sizeTran > USDO->sizeInd) ? CO_USDO_AB_DATA_LONG : CO_USDO_AB_DATA_SHORT;
                break;
            }

            uint8_t* dataTx;
            if (lastSegment) {
                dataTx = USDO_header(USDO, USDO->clientNodeId, (uint8_t)CO_USDO_CS_UL_SEG_END_RSP, USDO->sessionId);
                dataTx[3] = (uint8_t)count;
            } else {
                dataTx = USDO_header(USDO, USDO->clientNodeId, (uint8_t)CO_USDO_CS_UL_SEG_RSP, USDO->sessionId);
                dataTx[3] = counter;
            }
            (void)memcpy((void*)&dataTx[4], (const void*)&USDO->buf[USDO->bufOffsetRd], count);
            USDO->bufOffsetRd += count;
            USDO_send(USDO, (uint8_t)(4U + count));
            USDO->segCounter++;
            if (lastSegment) {
                USDO->state = CO_USDO_ST_IDLE;
            }
            break;
        }

        default: abortCode = CO_USDO_AB_SESSION; break;
    }

    if (abortCode != CO_USDO_AB_NONE) {
        USDO_abortCurrent(USDO, abortCode);
    }
}
#endif /* (CO_CONFIG_USDO) & CO_CONFIG_USDO_SRV_SEGMENTED */

/* Process one received USDO frame */
static void
USDO_processFrame(CO_USDOserver_t* USDO, const uint8_t* data, uint8_t len, uint8_t clientNodeId) {
    uint8_t cs = data[1];
    uint8_t sessionId = data[2];
    bool_t isCurrentSession = (USDO->state != CO_USDO_ST_IDLE) && (clientNodeId == USDO->clientNodeId)
                              && (sessionId == USDO->sessionId);

    /* abort from client */
    if (cs == (uint8_t)CO_USDO_CS_ABORT) {
        if (isCurrentSession) {
            USDO->state = CO_USDO_ST_IDLE;
        }
        return;
    }

    uint16_t index = (len >= 6U) ? (uint16_t)(((uint16_t)data[5] << 8) | data[4]) : 0U;
    uint8_t subIndex = (len >= 6U) ? data[3] : 0U;

    /* remote (routed) USDO services are not supported */
    if ((cs & (uint8_t)CO_USDO_CS_REMOTE) != 0U) {
        if (!isCurrentSession && (USDO->state != CO_USDO_ST_IDLE) && (clientNodeId == USDO->clientNodeId)) {
            USDO->state = CO_USDO_ST_IDLE;
        }
        USDO_abort(USDO, clientNodeId, sessionId, index, subIndex, CO_USDO_AB_ROUTING_NET);
        return;
    }

    bool_t isInitiate = (cs == (uint8_t)CO_USDO_CS_DL_EXP_REQ) || (cs == (uint8_t)CO_USDO_CS_UL_REQ)
                        || (cs == (uint8_t)CO_USDO_CS_DL_SEG_INIT_REQ) || (cs == (uint8_t)CO_USDO_CS_DL_BULK_INIT_REQ);

    if (isInitiate) {
        /* only one transfer at a time, new request from the same client aborts previous transfer */
        if ((USDO->state != CO_USDO_ST_IDLE) && (clientNodeId != USDO->clientNodeId)) {
            USDO_abort(USDO, clientNodeId, sessionId, index, subIndex, CO_USDO_AB_BUSY);
            return;
        }
        USDO->clientNodeId = clientNodeId;
        USDO->sessionId = sessionId;
        USDO->index = index;
        USDO->subIndex = subIndex;
        if (len < 6U) {
            USDO_abortCurrent(USDO, CO_USDO_AB_DLC);
            return;
        }
        USDO_processInitiate(USDO, data, len, cs);
    }
#if ((CO_CONFIG_USDO)&CO_CONFIG_USDO_SRV_SEGMENTED) != 0
    else if ((cs == (uint8_t)CO_USDO_CS_DL_SEG_REQ) || (cs == (uint8_t)CO_USDO_CS_DL_SEG_END_REQ)
             || (cs == (uint8_t)CO_USDO_CS_DL_BULK_SEG) || (cs == (uint8_t)CO_USDO_CS_DL_BULK_END_REQ)
             || (cs == (uint8_t)CO_USDO_CS_UL_SEG_REQ)) {
        if (isCurrentSession) {
            USDO_processSegment(USDO, data, len, cs);
        } else if ((USDO->state != CO_USDO_ST_IDLE) && (clientNodeId != USDO->clientNodeId)) {
            USDO_abort(USDO, clientNodeId, sessionId, 0, 0, CO_USDO_AB_BUSY);
        } else {
            /* segment without transfer or with wrong session-ID */
            if (USDO->state != CO_USDO_ST_IDLE) {
                USDO_abortCurrent(USDO, CO_USDO_AB_SESSION);
            } else {
                USDO_abort(USDO, clientNodeId, sessionId, 0, 0, CO_USDO_AB_SESSION);
            }
        }
    }
#endif
    else {
        if (isCurrentSession) {
            USDO_abortCurrent(USDO, CO_USDO_AB_CMD);
        } else if (USDO->state == CO_USDO_ST_IDLE) {
            USDO_abort(USDO, clientNodeId, sessionId, index, subIndex, CO_USDO_AB_CMD);
        } else { /* MISRA C 2004 14.10 */
        }
    }
}

bool_t
CO_USDOserver_process(CO_USDOserver_t* USDO, bool_t NMTisPreOrOperational, uint32_t timeDifference_us,
                      uint32_t* timerNext_us) {
    (void)timerNext_us; /* may be unused */

    if (USDO == NULL) {
        return false;
    }

    if (!NMTisPreOrOperational) {
        /* USDO is not available, clear everything */
        USDO->state = CO_USDO_ST_IDLE;
        for (uint8_t i = 0; i < (uint8_t)CO_CONFIG_USDO_SRV_RX_FIFO; i++) {
            CO_FLAG_CLEAR(USDO->CANrxNew[i]);
        }
        USDO->CANrxRd = 0;
        USDO->CANrxWr = 0;
        USDO->CANrxOverflow = false;
        return false;
    }

    /* Process received frames. Each frame may need transmission of the response. */
    while (CO_FLAG_READ(USDO->CANrxNew[USDO->CANrxRd]) && !USDO->CANtxBuff->bufferFull) {
        uint8_t slot = USDO->CANrxRd;
        USDO_processFrame(USDO, USDO->CANrxData[slot], USDO->CANrxLen[slot], USDO->CANrxClient[slot]);
        CO_FLAG_CLEAR(USDO->CANrxNew[slot]);
        slot++;
        USDO->CANrxRd = (slot < (uint8_t)CO_CONFIG_USDO_SRV_RX_FIFO) ? slot : 0U;
    }

    /* Frames were lost because of full fifo. Abort current transfer, if any. */
    if (USDO->CANrxOverflow && !USDO->CANtxBuff->bufferFull) {
        USDO->CANrxOverflow = false;
        if (USDO->state != CO_USDO_ST_IDLE) {
            USDO_abortCurrent(USDO, CO_USDO_AB_OUT_OF_MEM);
        }
    }

    /* timeout */
    if (USDO->state != CO_USDO_ST_IDLE) {
        if (USDO->timeoutTimer < USDO->timeoutTime_us) {
            USDO->timeoutTimer += timeDifference_us;
        }
        if ((USDO->timeoutTimer >= USDO->timeoutTime_us) && !USDO->CANtxBuff->bufferFull) {
            USDO_abortCurrent(USDO, CO_USDO_AB_TIMEOUT);
        }
#if ((CO_CONFIG_USDO)&CO_CONFIG_FLAG_TIMERNEXT) != 0
        else if (timerNext_us != NULL) {
            uint32_t diff = USDO->timeoutTime_us - USDO->timeoutTimer;
            if (*timerNext_us > diff) {
                *timerNext_us = diff;
            }
        } else { /* MISRA C 2004 14.10 */
        }
#endif
    }

    return (USDO->state != CO_USDO_ST_IDLE) || CO_FLAG_READ(USDO->CANrxNew[USDO->CANrxRd]);
}

#endif /* (CO_CONFIG_USDO) & CO_CONFIG_USDO_SRV_ENABLE */
