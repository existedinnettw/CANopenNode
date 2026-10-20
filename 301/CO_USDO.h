/**
 * CANopen FD Universal Service Data Object - common definitions.
 *
 * @file        CO_USDO.h
 * @ingroup     CO_USDO
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

#ifndef CO_USDO_H
#define CO_USDO_H

#include "301/CO_driver.h"
#include "301/CO_ODinterface.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_USDO
#if ((CO_CONFIG_CAN_FD)&CO_CONFIG_CAN_FD_ENABLE) != 0
#define CO_CONFIG_USDO                                                                                                 \
    (CO_CONFIG_USDO_SRV_ENABLE | CO_CONFIG_USDO_SRV_SEGMENTED | CO_CONFIG_USDO_SRV_BULK                                \
     | CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE | CO_CONFIG_GLOBAL_FLAG_TIMERNEXT)
#else
#define CO_CONFIG_USDO (0)
#endif
#endif

#if (((CO_CONFIG_USDO)&(CO_CONFIG_USDO_SRV_ENABLE | CO_CONFIG_USDO_CLI_ENABLE)) != 0) || defined CO_DOXYGEN

#if ((CO_CONFIG_CAN_FD)&CO_CONFIG_CAN_FD_ENABLE) == 0
#error USDO requires CO_CONFIG_CAN_FD
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_USDO USDO
 * CANopen FD Universal Service Data Object (CiA 1301).
 *
 * @ingroup CO_CANopen_301
 * @{
 * USDO provides access to the Object Dictionary of a CANopen FD device. It replaces SDO from CiA 301.
 *
 * USDO uses fixed CAN identifiers, which identify the sender of the CAN frame:
 * - USDO request from client: CAN-ID = 0x600 + node-ID of the client,
 * - USDO response from server: CAN-ID = 0x580 + node-ID of the server.
 *
 * The first byte of each USDO frame contains destination address: node-ID of the server inside request (or 0 for
 * broadcast) and node-ID of the client inside response. There are no configuration objects in the Object Dictionary.
 *
 * USDO frame layout (index is little-endian, sub-index is before index):
 *
 * | Service                        | Byte layout                                                             |
 * | ------------------------------ | ----------------------------------------------------------------------- |
 * | Expedited download / upload rsp| 0 DA, 1 CS, 2 SID, 3 sub, 4..5 index, 6 data type, 7 size n, 8.. data   |
 * | Upload req, download rsp       | 0 DA, 1 CS, 2 SID, 3 sub, 4..5 index                                    |
 * | Segmented / bulk initiate      | 0..5 as above, 6 data type, 7 reserved, 8..11 total size (0 = unknown)  |
 * | Bulk initiate rsp              | 0..5 as above, 6..7 segment processing time in 100 µs                   |
 * | Segment                        | 0 DA, 1 CS, 2 SID, 3 counter, 4..63 data (60 bytes)                     |
 * | Segment rsp, upload segment req| 0 DA, 1 CS, 2 SID, 3 counter                                            |
 * | End segment                    | 0 DA, 1 CS, 2 SID, 3 n, 4.. data (n bytes, 0 to 60)                     |
 * | End rsp                        | 0 DA, 1 CS, 2 SID                                                       |
 * | Abort                          | 0 DA, 1 CS=0x7F, 2 SID, 3 sub, 4..5 index, 6 abort code (1 byte)        |
 *
 * Segment counter starts with 0 in the first segment and is incremented in each segment.
 *
 * Remote (routed) USDO services (command specifiers with bit 7 set) are not supported, server responds with abort
 * @ref CO_USDO_AB_ROUTING_NET.
 */

/** Maximum data size in expedited USDO transfer */
#define CO_USDO_EXPEDITED_MAX 56U
/** Number of data bytes in USDO segment */
#define CO_USDO_SEGMENT_SIZE  60U
/** Length of the CAN frame with USDO segment */
#define CO_USDO_SEGMENT_LEN   64U
/** Base CAN identifier of USDO request (+node-ID of the client) */
#define CO_CAN_ID_USDO_REQ    CO_CAN_ID_SDO_CLI
/** Base CAN identifier of USDO response (+node-ID of the server) */
#define CO_CAN_ID_USDO_RSP    CO_CAN_ID_SDO_SRV

/**
 * USDO command specifiers, byte 1 of the USDO frame
 */
typedef enum {
    CO_USDO_CS_DL_EXP_REQ = 0x01U,       /**< Download expedited request */
    CO_USDO_CS_DL_SEG_INIT_REQ = 0x02U,  /**< Download segmented initiate request */
    CO_USDO_CS_DL_SEG_REQ = 0x03U,       /**< Download segment request */
    CO_USDO_CS_DL_SEG_END_REQ = 0x04U,   /**< Download segmented end request */
    CO_USDO_CS_DL_BULK_INIT_REQ = 0x06U, /**< Download bulk initiate request */
    CO_USDO_CS_DL_BULK_SEG = 0x07U,      /**< Download bulk segment (not confirmed) */
    CO_USDO_CS_DL_BULK_END_REQ = 0x08U,  /**< Download bulk end request */
    CO_USDO_CS_UL_REQ = 0x11U,           /**< Upload request */
    CO_USDO_CS_UL_SEG_REQ = 0x13U,       /**< Upload segment request */
    CO_USDO_CS_DL_EXP_RSP = 0x21U,       /**< Download expedited response */
    CO_USDO_CS_DL_SEG_INIT_RSP = 0x22U,  /**< Download segmented initiate response */
    CO_USDO_CS_DL_SEG_RSP = 0x23U,       /**< Download segment response */
    CO_USDO_CS_DL_SEG_END_RSP = 0x24U,   /**< Download segmented end response */
    CO_USDO_CS_DL_BULK_INIT_RSP = 0x26U, /**< Download bulk initiate response */
    CO_USDO_CS_DL_BULK_END_RSP = 0x28U,  /**< Download bulk end response */
    CO_USDO_CS_UL_EXP_RSP = 0x31U,       /**< Upload expedited response */
    CO_USDO_CS_UL_SEG_INIT_RSP = 0x32U,  /**< Upload segmented initiate response */
    CO_USDO_CS_UL_SEG_RSP = 0x33U,       /**< Upload segment response */
    CO_USDO_CS_UL_SEG_END_RSP = 0x34U,   /**< Upload segmented end response */
    CO_USDO_CS_ABORT = 0x7FU,            /**< Abort (request or response) */
    CO_USDO_CS_REMOTE = 0x80U            /**< Bit indicates remote (routed) USDO service */
} CO_USDO_CS_t;

/**
 * USDO abort codes (one byte), byte 6 of the USDO abort frame
 */
typedef enum {
    CO_USDO_AB_NONE = 0x00U,           /**< 0x00, No abort */
    CO_USDO_AB_GENERAL = 0x10U,        /**< 0x10, Unknown or general error */
    CO_USDO_AB_PROTOCOL = 0x11U,       /**< 0x11, USDO protocol error */
    CO_USDO_AB_TIMEOUT = 0x12U,        /**< 0x12, USDO protocol timed out */
    CO_USDO_AB_CMD = 0x13U,            /**< 0x13, Command specifier unknown */
    CO_USDO_AB_SEG_COUNTER = 0x14U,    /**< 0x14, Wrong segment counter */
    CO_USDO_AB_DATA_SIZE = 0x15U,      /**< 0x15, Wrong data size */
    CO_USDO_AB_OUT_OF_MEM = 0x16U,     /**< 0x16, Out of memory */
    CO_USDO_AB_ROUTING = 0x19U,        /**< 0x19, Routing general error */
    CO_USDO_AB_ROUTING_NET = 0x1AU,    /**< 0x1A, Routing: destination network not reachable */
    CO_USDO_AB_ROUTING_NODE = 0x1BU,   /**< 0x1B, Routing: destination node not reachable */
    CO_USDO_AB_DLC = 0x1EU,            /**< 0x1E, Unexpected data length of the CAN frame */
    CO_USDO_AB_SESSION = 0x21U,        /**< 0x21, Session-ID wrong or unknown */
    CO_USDO_AB_UNSUPP_ACCESS = 0x30U,  /**< 0x30, Unsupported access to an object */
    CO_USDO_AB_WRITEONLY = 0x31U,      /**< 0x31, Attempt to read a write only object */
    CO_USDO_AB_READONLY = 0x32U,       /**< 0x32, Attempt to write a read only object */
    CO_USDO_AB_NOT_EXIST = 0x33U,      /**< 0x33, Object does not exist */
    CO_USDO_AB_SUB_UNKNOWN = 0x34U,    /**< 0x34, Sub-index does not exist */
    CO_USDO_AB_HW = 0x35U,             /**< 0x35, Access failed due to hardware error */
    CO_USDO_AB_TYPE_MISMATCH = 0x36U,  /**< 0x36, Data type does not match, length does not match */
    CO_USDO_AB_DATA_LONG = 0x37U,      /**< 0x37, Data type does not match, length too high */
    CO_USDO_AB_DATA_SHORT = 0x38U,     /**< 0x38, Data type does not match, length too low */
    CO_USDO_AB_INVALID_VALUE = 0x40U,  /**< 0x40, Invalid value for parameter */
    CO_USDO_AB_VALUE_HIGH = 0x41U,     /**< 0x41, Value of parameter written too high */
    CO_USDO_AB_VALUE_LOW = 0x42U,      /**< 0x42, Value of parameter written too low */
    CO_USDO_AB_MAX_LESS_MIN = 0x43U,   /**< 0x43, Maximum value is less than minimum value */
    CO_USDO_AB_NO_MAP = 0x50U,         /**< 0x50, Object cannot be mapped to the PDO */
    CO_USDO_AB_MAP_LEN = 0x51U,        /**< 0x51, PDO length exceeded */
    CO_USDO_AB_DATA_TRANSF = 0x60U,    /**< 0x60, Data cannot be transferred or stored */
    CO_USDO_AB_DATA_LOC_CTRL = 0x61U,  /**< 0x61, Data cannot be transferred because of local control */
    CO_USDO_AB_DATA_DEV_STATE = 0x62U, /**< 0x62, Data cannot be transferred because of present device state */
    CO_USDO_AB_DATA_OD = 0x63U,        /**< 0x63, Object dictionary not present or generation failed */
    CO_USDO_AB_NO_DATA = 0x64U,        /**< 0x64, No data available */
    CO_USDO_AB_BUSY = 0x70U            /**< 0x70, Server is busy */
} CO_USDO_abortCode_t;

/**
 * CANopen data types (index of the data type object), used in byte 6 of USDO frame
 */
typedef enum {
    CO_USDO_DT_UNKNOWN = 0x00U,        /**< Data type not specified */
    CO_USDO_DT_BOOLEAN = 0x01U,        /**< BOOLEAN */
    CO_USDO_DT_INTEGER8 = 0x02U,       /**< INTEGER8 */
    CO_USDO_DT_INTEGER16 = 0x03U,      /**< INTEGER16 */
    CO_USDO_DT_INTEGER32 = 0x04U,      /**< INTEGER32 */
    CO_USDO_DT_UNSIGNED8 = 0x05U,      /**< UNSIGNED8 */
    CO_USDO_DT_UNSIGNED16 = 0x06U,     /**< UNSIGNED16 */
    CO_USDO_DT_UNSIGNED32 = 0x07U,     /**< UNSIGNED32 */
    CO_USDO_DT_REAL32 = 0x08U,         /**< REAL32 */
    CO_USDO_DT_VISIBLE_STRING = 0x09U, /**< VISIBLE_STRING */
    CO_USDO_DT_OCTET_STRING = 0x0AU,   /**< OCTET_STRING */
    CO_USDO_DT_UNICODE_STRING = 0x0BU, /**< UNICODE_STRING */
    CO_USDO_DT_DOMAIN = 0x0FU,         /**< DOMAIN */
    CO_USDO_DT_UNSIGNED24 = 0x16U,     /**< UNSIGNED24 */
    CO_USDO_DT_UNSIGNED64 = 0x1BU      /**< UNSIGNED64 */
} CO_USDO_dataType_t;

/**
 * Translate OD read/write return code into USDO abort code.
 *
 * @param returnCode Return code from OD read/write function.
 *
 * @return USDO abort code.
 */
CO_USDO_abortCode_t CO_USDO_abortCodeFromODR(ODR_t returnCode);

/** @} */ /* CO_USDO */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_USDO) & (CO_CONFIG_USDO_SRV_ENABLE | CO_CONFIG_USDO_CLI_ENABLE) */

#endif /* CO_USDO_H */
