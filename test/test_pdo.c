/*
 * Unit test for RPDO and TPDO, classic CAN and CAN FD (CiA 1301), byte-wise and bitwise mapping.
 *
 * Test builds own small Object Dictionary, maps variables 0x2000+ into TPDO and 0x2100+ into RPDO, sends TPDO, loops
 * the frame back into RPDO and verifies the values. It also verifies frame length, CAN FD padding and errors for too
 * short or too long RPDO frames.
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
#define OD_DEFINITION
#include <stdio.h>
#include "301/CO_PDO.h"
#include "test_driver.h"

#define NVARS 16
static uint32_t varTx[NVARS];
static uint32_t varRx[NVARS];
static uint8_t errReg;
static uint32_t cobIdEm = 0x80 + 5;

static uint8_t rpdoMaxSub = 5, tpdoMaxSub = 6;
static uint32_t rpdoCob = 0x205, tpdoCob = 0x185;
static uint8_t rpdoTT = 254, tpdoTT = 254;
static uint16_t rpdoEvt = 0, tpdoInh = 0, tpdoEvt = 0;
static uint8_t tpdoSyncStart = 0;
static uint8_t rpdoMapCnt, tpdoMapCnt;
static uint32_t rpdoMap[NVARS], tpdoMap[NVARS];

static OD_obj_var_t oVarTx[NVARS], oVarRx[NVARS];
static OD_obj_var_t o1001 = {&errReg, ODA_SDO_R, 1};
static OD_obj_var_t o1014 = {&cobIdEm, ODA_SDO_RW | ODA_MB, 4};
static OD_obj_record_t o1400[] = {
    {&rpdoMaxSub, 0, ODA_SDO_R, 1}, {&rpdoCob, 1, ODA_SDO_RW | ODA_MB, 4},
    {&rpdoTT, 2, ODA_SDO_RW, 1},    {&rpdoEvt, 5, ODA_SDO_RW | ODA_MB, 2}};
static OD_obj_record_t o1800[] = {{&tpdoMaxSub, 0, ODA_SDO_R, 1},         {&tpdoCob, 1, ODA_SDO_RW | ODA_MB, 4},
                                  {&tpdoTT, 2, ODA_SDO_RW, 1},            {&tpdoInh, 3, ODA_SDO_RW | ODA_MB, 2},
                                  {&tpdoEvt, 5, ODA_SDO_RW | ODA_MB, 2},  {&tpdoSyncStart, 6, ODA_SDO_RW, 1}};
static OD_obj_record_t o1600[NVARS + 1], o1A00[NVARS + 1];
static OD_extension_t extTx[NVARS];

static OD_entry_t odList[2 * NVARS + 7];
static OD_t od;

static void
buildOD(void) {
    int n = 0;
    odList[n++] = (OD_entry_t){0x1001, 1, ODT_VAR, &o1001, NULL};
    odList[n++] = (OD_entry_t){0x1014, 1, ODT_VAR, &o1014, NULL};
    o1600[0] = (OD_obj_record_t){&rpdoMapCnt, 0, ODA_SDO_RW, 1};
    o1A00[0] = (OD_obj_record_t){&tpdoMapCnt, 0, ODA_SDO_RW, 1};
    for (int i = 0; i < NVARS; i++) {
        o1600[i + 1] = (OD_obj_record_t){&rpdoMap[i], (uint8_t)(i + 1), ODA_SDO_RW | ODA_MB, 4};
        o1A00[i + 1] = (OD_obj_record_t){&tpdoMap[i], (uint8_t)(i + 1), ODA_SDO_RW | ODA_MB, 4};
    }
    odList[n++] = (OD_entry_t){0x1400, 4, ODT_REC, o1400, NULL};
    odList[n++] = (OD_entry_t){0x1600, NVARS + 1, ODT_REC, o1600, NULL};
    odList[n++] = (OD_entry_t){0x1800, 6, ODT_REC, o1800, NULL};
    odList[n++] = (OD_entry_t){0x1A00, NVARS + 1, ODT_REC, o1A00, NULL};
    for (int i = 0; i < NVARS; i++) {
        oVarTx[i] = (OD_obj_var_t){&varTx[i], ODA_SDO_RW | ODA_TRPDO | ODA_MB, 4};
        extTx[i].read = OD_readOriginal;
        extTx[i].write = OD_writeOriginal;
        odList[n++] = (OD_entry_t){(uint16_t)(0x2000 + i), 1, ODT_VAR, &oVarTx[i], &extTx[i]};
    }
    for (int i = 0; i < NVARS; i++) {
        oVarRx[i] = (OD_obj_var_t){&varRx[i], ODA_SDO_RW | ODA_TRPDO | ODA_MB, 4};
        odList[n++] = (OD_entry_t){(uint16_t)(0x2100 + i), 1, ODT_VAR, &oVarRx[i], NULL};
    }
    odList[n] = (OD_entry_t){0, 0, 0, NULL, NULL};
    od.size = (uint16_t)n;
    od.list = odList;
}

static CO_CANmodule_t CANmod;
static CO_CANrx_t rxArr[4];
static CO_CANtx_t txArr[4];
static CO_EM_t em;
static CO_EM_fifo_t emFifo[8];
static CO_RPDO_t RPDO;
static CO_TPDO_t TPDO;

/* configure mapping: bits[i] mapped from 0x2000+i (TPDO) and 0x2100+i (RPDO) */
static void
setup(int n, const uint8_t* bits) {
    uint32_t errInfo = 0;
    memset(varTx, 0, sizeof(varTx));
    memset(varRx, 0, sizeof(varRx));
    rpdoMapCnt = tpdoMapCnt = (uint8_t)n;
    for (int i = 0; i < n; i++) {
        tpdoMap[i] = ((uint32_t)(0x2000 + i) << 16) | bits[i];
        rpdoMap[i] = ((uint32_t)(0x2100 + i) << 16) | bits[i];
    }
    CO_CANmodule_init(&CANmod, NULL, rxArr, 4, txArr, 4, 125);
    CHECK(CO_EM_init(&em, &CANmod, &odList[0], emFifo, 8, &odList[1], 0,
#if ((CO_CONFIG_EM)&CO_CONFIG_EM_HISTORY) != 0
                     NULL,
#endif
                     5, &errInfo)
          == CO_ERROR_NO);
    CHECK(CO_RPDO_init(&RPDO, &od, &em, NULL, 0x205, &odList[2], &odList[3], &CANmod, 0, &errInfo) == CO_ERROR_NO);
    CHECK(CO_TPDO_init(&TPDO, &od, &em, NULL, 0x185, &odList[4], &odList[5], &CANmod, 1, &errInfo) == CO_ERROR_NO);
    CHECK(RPDO.PDO_common.valid);
    CHECK(TPDO.PDO_common.valid);
    test_txCount = 0;
}

static void
processRPDO(void) {
    bool_t to = false;
    uint32_t next = 0;
    CO_RPDO_process(&RPDO, 1000, &to, &next, true, false);
}

static int
totalBits(int n, const uint8_t* bits) {
    int t = 0;
    for (int i = 0; i < n; i++) {
        t += bits[i];
    }
    return t;
}

static void
roundtrip(const char* name, int n, const uint8_t* bits) {
    printf("-- %s\n", name);
    setup(n, bits);
    int byteLen = (totalBits(n, bits) + 7) / 8;
    for (int i = 0; i < n; i++) {
        varTx[i] = 0xA5C3F00Fu * (uint32_t)(i + 1) + 0x01020304u;
    }
    TPDO.sendRequest = true;
    uint32_t next = 0;
    CO_TPDO_process(&TPDO, 0, &next, true, false);
    CHECK(test_txCount == 1);
    test_frame_t* f = &test_txLog[0];
    CHECK(f->ident == 0x185);
    CHECK(f->len == CO_CANframeLength((uint8_t)byteLen));
    for (int i = byteLen; i < f->len; i++) {
        CHECK(f->data[i] == CO_CAN_FD_PADDING_BYTE);
    }
    printf("   PDO %d bytes, frame %d bytes:", byteLen, f->len);
    for (int i = 0; i < f->len; i++) {
        printf(" %02X", f->data[i]);
    }
    printf("\n");

    /* loop back */
    test_receive(&CANmod, 0x205, f->data, f->len);
    processRPDO();
    for (int i = 0; i < n; i++) {
        uint32_t mask = bits[i] >= 32 ? 0xFFFFFFFFu : ((1u << bits[i]) - 1u);
        if (varRx[i] != (varTx[i] & mask)) {
            printf("   var %d: tx %08X rx %08X mask %08X\n", i, varTx[i], varRx[i], mask);
        }
        CHECK(varRx[i] == (varTx[i] & mask));
    }
    CHECK(!CO_isError(&em, CO_EM_RPDO_WRONG_LENGTH));

    /* too short frame: error, data not processed */
    if (byteLen > 0) {
        uint8_t shortLen = (uint8_t)(byteLen - 1);
        uint8_t d[64] = {0};
        memset(varRx, 0, sizeof(varRx));
        test_receive(&CANmod, 0x205, d, shortLen);
        processRPDO();
        CHECK(CO_isError(&em, CO_EM_RPDO_WRONG_LENGTH));
        CHECK(varRx[0] == 0);
        /* correct frame clears error */
        test_receive(&CANmod, 0x205, f->data, f->len);
        processRPDO();
        CHECK(!CO_isError(&em, CO_EM_RPDO_WRONG_LENGTH));
    }
    /* too long frame: error, but processed */
    if (f->len < CO_CAN_DATA_MAX_LEN) {
        uint8_t d[64];
        memset(d, 0, sizeof(d));
        memcpy(d, f->data, f->len);
        uint8_t longLen = CO_CANframeLength((uint8_t)(f->len + 1));
        memset(varRx, 0, sizeof(varRx));
        test_receive(&CANmod, 0x205, d, longLen);
        processRPDO();
        CHECK(CO_isError(&em, CO_EM_RPDO_WRONG_LENGTH));
        CHECK(n == 0 || varRx[n - 1] != 0 || varTx[n - 1] == 0);
    }
}

int
main(void) {
    buildOD();
    printf("CO_CAN_DATA_MAX_LEN=%u CO_PDO_MAX_SIZE=%u bitwise=%d\n", (unsigned)CO_CAN_DATA_MAX_LEN,
           (unsigned)CO_PDO_MAX_SIZE, ((CO_CONFIG_PDO)&CO_CONFIG_PDO_BITWISE_MAPPING) != 0);

    static const uint8_t b2x32[] = {32, 32};
    roundtrip("2 x 32 bit (8 bytes)", 2, b2x32);
    static const uint8_t b1x16[] = {16};
    roundtrip("1 x 16 bit", 1, b1x16);
#if CO_PDO_MAX_SIZE > 8
    static const uint8_t b11x32[] = {32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32};
    roundtrip("11 x 32 bit (44 bytes -> 48)", 11, b11x32);
    static const uint8_t b16x32[] = {32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32};
    roundtrip("16 x 32 bit (64 bytes)", 16, b16x32);
    static const uint8_t b3x24[] = {24, 24, 24, 8};
    roundtrip("3x24+8 bit (10 bytes -> 12)", 4, b3x24);
#endif
#if ((CO_CONFIG_PDO)&CO_CONFIG_PDO_BITWISE_MAPPING) != 0
    static const uint8_t bbits[] = {3, 5, 1, 7, 16, 9, 23};
    roundtrip("bitwise 64 bits", 7, bbits);
    static const uint8_t bbits2[] = {3, 13, 1};
    roundtrip("bitwise 17 bits", 3, bbits2);
#if CO_PDO_MAX_SIZE > 8
    static const uint8_t bbits3[] = {3, 32, 1, 31, 17, 32, 5, 9, 27, 32, 32, 32, 32, 30, 2, 31};
    roundtrip("bitwise 350 bits", 16, bbits3);
    static const uint8_t bbits4[] = {7, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32};
    roundtrip("bitwise 487 bits", 16, bbits4);
#endif
#endif
    printf("%s: %d failures\n", test_failures ? "FAILED" : "PASSED", test_failures);
    return test_failures != 0;
}
