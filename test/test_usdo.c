/*
 * Unit test for USDO server and client and CANopen FD emergency message (CiA 1301).
 *
 * USDO client (node 10) and server (node 5) communicate over simulated CAN FD bus. Raw frames are also injected into
 * the server to test protocol details: busy, segment counter, remote services, broadcast, bulk download, timeout.
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
#include "301/CO_USDOserver.h"
#include "301/CO_USDOclient.h"
#include "301/CO_Emergency.h"
#include "test_driver.h"

#define SRV_ID 5
#define CLI_ID 10

static uint8_t errReg;
static uint32_t cobIdEm = 0x80;
static uint32_t v2000;
static uint8_t v2001[200];
static char v2002[100];
static uint16_t v2003 = 0xBEEF;
static uint8_t v2005[3] = {1, 2, 3};

static OD_obj_var_t o1001 = {&errReg, ODA_SDO_R, 1};
static OD_obj_var_t o1014 = {&cobIdEm, ODA_SDO_RW | ODA_MB, 4};
static OD_obj_var_t o2000 = {&v2000, ODA_SDO_RW | ODA_MB, 4};
static OD_obj_var_t o2001 = {v2001, ODA_SDO_RW, sizeof(v2001)};
static OD_obj_var_t o2002 = {v2002, ODA_SDO_RW | ODA_STR, sizeof(v2002)};
static OD_obj_var_t o2003 = {&v2003, ODA_SDO_R | ODA_MB, 2};
static OD_obj_record_t o2005[] = {{&(uint8_t){2}, 0, ODA_SDO_R, 1}, {&v2005[0], 1, ODA_SDO_RW, 1}, {&v2005[1], 2, ODA_SDO_RW, 1}};
static OD_entry_t odList[] = {
    {0x1001, 1, ODT_VAR, &o1001, NULL}, {0x1014, 1, ODT_VAR, &o1014, NULL}, {0x2000, 1, ODT_VAR, &o2000, NULL},
    {0x2001, 1, ODT_VAR, &o2001, NULL}, {0x2002, 1, ODT_VAR, &o2002, NULL}, {0x2003, 1, ODT_VAR, &o2003, NULL},
    {0x2005, 3, ODT_REC, o2005, NULL},  {0, 0, 0, NULL, NULL}};
static OD_t od = {7, odList};

static CO_CANmodule_t modSrv, modCli;
static CO_CANrx_t rxSrv[4], rxCli[4];
static CO_CANtx_t txSrv[4], txCli[4];
static CO_USDOserver_t srv;
static CO_USDOclient_t cli;
static CO_EM_t em;
static CO_EM_fifo_t emFifo[8];

static CO_USDO_return_t
pump(CO_USDO_abortCode_t* ab, size_t* size) {
    CO_USDO_return_t ret = CO_USDO_RT_waitingResponse;
    for (int i = 0; i < 1000 && ret == CO_USDO_RT_waitingResponse; i++) {
        CO_USDOserver_process(&srv, true, 1000, NULL);
        ret = CO_USDOclient_process(&cli, 1000, NULL, ab, size);
    }
    return ret;
}

static CO_USDO_return_t
download(uint16_t idx, uint8_t sub, const void* data, size_t size, CO_USDO_abortCode_t* ab) {
    CO_USDO_return_t ret = CO_USDOclient_download(&cli, SRV_ID, idx, sub, 0, data, size, 500);
    CHECK(ret == CO_USDO_RT_waitingResponse);
    return pump(ab, NULL);
}

static CO_USDO_return_t
upload(uint16_t idx, uint8_t sub, void* buf, size_t bufSize, size_t* size, CO_USDO_abortCode_t* ab) {
    CO_USDO_return_t ret = CO_USDOclient_upload(&cli, SRV_ID, idx, sub, buf, bufSize, 500);
    CHECK(ret == CO_USDO_RT_waitingResponse);
    return pump(ab, size);
}

/* inject raw frame from client nodeId to server, process server, return last tx frame */
static test_frame_t*
inject(uint8_t clientId, const uint8_t* data, uint8_t len) {
    int before = test_txCount;
    test_receive(&modSrv, (uint16_t)(0x600 + clientId), data, len);
    CO_USDOserver_process(&srv, true, 1000, NULL);
    return (test_txCount > before) ? &test_txLog[test_txCount - 1] : NULL;
}

int
main(void) {
    uint32_t errInfo;
    CO_USDO_abortCode_t ab;
    size_t size;
    uint8_t buf[300];

    CO_CANmodule_init(&modSrv, NULL, rxSrv, 4, txSrv, 4, 500);
    CO_CANmodule_init(&modCli, NULL, rxCli, 4, txCli, 4, 500);
    CHECK(CO_USDOserver_init(&srv, &od, SRV_ID, 1000, &modSrv, 0, &modSrv, 0, &errInfo) == CO_ERROR_NO);
    CHECK(CO_USDOclient_init(&cli, CLI_ID, &modCli, 0, &modCli, 0) == CO_ERROR_NO);
    test_busEnabled = true;

    printf("-- expedited u32\n");
    uint32_t val = 0x12345678;
    CHECK(download(0x2000, 0, &val, 4, &ab) == CO_USDO_RT_ok_communicationEnd);
    CHECK(v2000 == 0x12345678);
    uint32_t rd = 0;
    CHECK(upload(0x2000, 0, &rd, 4, &size, &ab) == CO_USDO_RT_ok_communicationEnd);
    CHECK(rd == 0x12345678 && size == 4);
    CHECK(cli.dataType == CO_USDO_DT_UNSIGNED32);
    /* check expedited upload frame: 0x585, 12 bytes (8+4) */
    test_frame_t* f = &test_txLog[test_txCount - 1];
    CHECK(f->ident == 0x585 && f->len == 12 && f->data[0] == CLI_ID && f->data[1] == 0x31 && f->data[3] == 0
          && f->data[4] == 0x00 && f->data[5] == 0x20 && f->data[6] == 0x07 && f->data[7] == 4);

    printf("-- segmented 200 bytes\n");
    uint8_t pattern[200];
    for (int i = 0; i < 200; i++) {
        pattern[i] = (uint8_t)(i * 7 + 3);
    }
    CHECK(download(0x2001, 0, pattern, 200, &ab) == CO_USDO_RT_ok_communicationEnd);
    CHECK(memcmp(v2001, pattern, 200) == 0);
    memset(buf, 0, sizeof(buf));
    CHECK(upload(0x2001, 0, buf, sizeof(buf), &size, &ab) == CO_USDO_RT_ok_communicationEnd);
    CHECK(size == 200 && memcmp(buf, pattern, 200) == 0);
    CHECK(cli.dataType == CO_USDO_DT_OCTET_STRING);

    printf("-- strings\n");
    CHECK(download(0x2002, 0, "hello", 5, &ab) == CO_USDO_RT_ok_communicationEnd);
    CHECK(strcmp(v2002, "hello") == 0);
    memset(buf, 0, sizeof(buf));
    CHECK(upload(0x2002, 0, buf, sizeof(buf), &size, &ab) == CO_USDO_RT_ok_communicationEnd);
    CHECK(size == 5 && memcmp(buf, "hello", 5) == 0);
    CHECK(cli.dataType == CO_USDO_DT_VISIBLE_STRING);
    const char* longStr = "0123456789abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-------";
    CHECK(download(0x2002, 0, longStr, strlen(longStr), &ab) == CO_USDO_RT_ok_communicationEnd);
    CHECK(strcmp(v2002, longStr) == 0);
    memset(buf, 0, sizeof(buf));
    CHECK(upload(0x2002, 0, buf, sizeof(buf), &size, &ab) == CO_USDO_RT_ok_communicationEnd);
    CHECK(size == strlen(longStr) && memcmp(buf, longStr, size) == 0);

    printf("-- aborts\n");
    uint16_t u16 = 1;
    CHECK(download(0x2003, 0, &u16, 2, &ab) == CO_USDO_RT_endedWithServerAbort && ab == CO_USDO_AB_READONLY);
    CHECK(upload(0x2004, 0, buf, 10, &size, &ab) == CO_USDO_RT_endedWithServerAbort && ab == CO_USDO_AB_NOT_EXIST);
    CHECK(upload(0x2005, 7, buf, 10, &size, &ab) == CO_USDO_RT_endedWithServerAbort && ab == CO_USDO_AB_SUB_UNKNOWN);
    CHECK(download(0x2000, 0, pattern, 5, &ab) == CO_USDO_RT_endedWithServerAbort && ab == CO_USDO_AB_DATA_LONG);
    CHECK(download(0x2000, 0, pattern, 3, &ab) == CO_USDO_RT_endedWithServerAbort && ab == CO_USDO_AB_DATA_SHORT);
    CHECK(download(0x2001, 0, pattern, 100, &ab) == CO_USDO_RT_endedWithServerAbort && ab == CO_USDO_AB_DATA_SHORT);
    CHECK(upload(0x2001, 0, buf, 100, &size, &ab) == CO_USDO_RT_endedWithClientAbort && ab == CO_USDO_AB_OUT_OF_MEM);
    CO_USDOserver_process(&srv, true, 1000, NULL); /* process abort from client */
    CHECK(srv.state == CO_USDO_ST_IDLE);
    CHECK(upload(0x2003, 0, &u16, 2, &size, &ab) == CO_USDO_RT_ok_communicationEnd && u16 == 0xBEEF);
    uint8_t u8 = 9;
    CHECK(download(0x2005, 2, &u8, 1, &ab) == CO_USDO_RT_ok_communicationEnd && v2005[1] == 9);

    printf("-- raw frames: busy, counter, remote, broadcast, bulk, timeout\n");
    test_busEnabled = false;
    /* segmented download initiate from node 20, size 130 */
    uint8_t r[64];
    memset(r, 0, sizeof(r));
    r[0] = SRV_ID; r[1] = 0x02; r[2] = 7; r[3] = 0; r[4] = 0x01; r[5] = 0x20; r[6] = 0x0A; r[8] = 200;
    f = inject(20, r, 12);
    CHECK(f && f->ident == 0x585 && f->len == 6 && f->data[0] == 20 && f->data[1] == 0x22 && f->data[2] == 7);
    /* another client is rejected with busy */
    uint8_t r2[8] = {SRV_ID, 0x11, 1, 0, 0x00, 0x20};
    f = inject(21, r2, 6);
    CHECK(f && f->data[0] == 21 && f->data[1] == 0x7F && f->data[6] == CO_USDO_AB_BUSY);
    CHECK(srv.state == CO_USDO_ST_DL_SEG);
    /* segment with wrong counter */
    uint8_t seg[64];
    memset(seg, 0xAB, sizeof(seg));
    seg[0] = SRV_ID; seg[1] = 0x03; seg[2] = 7; seg[3] = 1;
    f = inject(20, seg, 64);
    CHECK(f && f->data[0] == 20 && f->data[1] == 0x7F && f->data[2] == 7 && f->data[6] == CO_USDO_AB_SEG_COUNTER);
    CHECK(srv.state == CO_USDO_ST_IDLE);

    /* remote command */
    uint8_t r3[8] = {SRV_ID, 0x81, 1, 0, 0x00, 0x20};
    f = inject(20, r3, 6);
    CHECK(f && f->data[1] == 0x7F && f->data[6] == CO_USDO_AB_ROUTING_NET);

    /* broadcast expedited upload */
    uint8_t r4[8] = {0, 0x11, 9, 0, 0x03, 0x20};
    f = inject(30, r4, 6);
    CHECK(f && f->ident == 0x585 && f->data[0] == 30 && f->data[1] == 0x31 && f->data[2] == 9 && f->data[7] == 2
          && f->data[8] == 0xEF && f->data[9] == 0xBE && f->len == 12);
    /* request for other node is ignored */
    uint8_t r5[8] = {6, 0x11, 9, 0, 0x03, 0x20};
    CHECK(inject(30, r5, 6) == NULL);
    /* broadcast segmented is not allowed */
    r[0] = 0;
    f = inject(30, r, 12);
    CHECK(f && f->data[1] == 0x7F && f->data[6] == CO_USDO_AB_PROTOCOL);

    /* bulk download 130 bytes: 60 + 60 + 10 */
    memset(r, 0, sizeof(r));
    r[0] = SRV_ID; r[1] = 0x06; r[2] = 3; r[3] = 0; r[4] = 0x01; r[5] = 0x20; r[8] = 200;
    f = inject(20, r, 12);
    CHECK(f && f->data[1] == 0x26 && f->len == 8 && f->data[6] == CO_CONFIG_USDO_SRV_BULK_TIME && f->data[7] == 0);
    /* three segments + end without processing in between (fifo of 4) */
    int before = test_txCount;
    for (int s = 0; s < 3; s++) {
        seg[0] = SRV_ID; seg[1] = 0x07; seg[2] = 3; seg[3] = (uint8_t)s;
        for (int i = 0; i < 60; i++) seg[4 + i] = (uint8_t)(s * 60 + i);
        test_receive(&modSrv, 0x614, seg, 64);
    }
    uint8_t end[24] = {SRV_ID, 0x08, 3, 20};
    for (int i = 0; i < 20; i++) end[4 + i] = (uint8_t)(180 + i);
    test_receive(&modSrv, 0x614, end, 24);
    CO_USDOserver_process(&srv, true, 1000, NULL);
    CHECK(test_txCount == before + 1);
    f = &test_txLog[test_txCount - 1];
    CHECK(f->data[1] == 0x28 && f->len == 3);
    int ok = 1;
    for (int i = 0; i < 200; i++) ok &= v2001[i] == (uint8_t)i;
    CHECK(ok);
    /* fifo overflow in bulk transfer */
    memset(r, 0, sizeof(r));
    r[0] = SRV_ID; r[1] = 0x06; r[2] = 4; r[4] = 0x01; r[5] = 0x20; r[8] = 200;
    f = inject(20, r, 12);
    for (int s = 0; s < 5; s++) {
        seg[2] = 4; seg[3] = (uint8_t)s;
        test_receive(&modSrv, 0x614, seg, 64);
    }
    CO_USDOserver_process(&srv, true, 1000, NULL);
    f = &test_txLog[test_txCount - 1];
    CHECK(f->data[1] == 0x7F && (f->data[6] == CO_USDO_AB_OUT_OF_MEM || f->data[6] == CO_USDO_AB_DATA_LONG));
    CHECK(srv.state == CO_USDO_ST_IDLE);
    CHECK(!srv.CANrxOverflow);

    /* timeout */
    memset(r, 0, sizeof(r));
    r[0] = SRV_ID; r[1] = 0x02; r[2] = 8; r[4] = 0x01; r[5] = 0x20; r[8] = 200;
    f = inject(20, r, 12);
    CHECK(srv.state == CO_USDO_ST_DL_SEG);
    for (int i = 0; i < 1100; i++) CO_USDOserver_process(&srv, true, 1000, NULL);
    f = &test_txLog[test_txCount - 1];
    CHECK(f->data[1] == 0x7F && f->data[2] == 8 && f->data[6] == CO_USDO_AB_TIMEOUT && srv.state == CO_USDO_ST_IDLE);

    /* expedited download with padding: 8 + 13 = 21 -> 24 */
    uint8_t r6[24] = {SRV_ID, 0x01, 5, 0, 0x02, 0x20, 0x09, 3, 'a', 'b', 'c'};
    f = inject(20, r6, 12);
    CHECK(f && f->data[1] == 0x21 && strcmp(v2002, "abc") == 0);

    printf("-- EMCY FD\n");
    test_busEnabled = false;
    CHECK(CO_EM_init(&em, &modSrv, &odList[0], emFifo, 8, &odList[1], 1, SRV_ID, &errInfo) == CO_ERROR_NO);
    CO_errorReport(&em, CO_EM_GENERIC_ERROR, CO_EMC_GENERIC, 0xA1B2C3D4);
    before = test_txCount;
    CO_EM_process(&em, true, 1000, NULL);
    CHECK(test_txCount == before + 1);
    f = &test_txLog[test_txCount - 1];
    CHECK(f->ident == 0x85 && f->len == 20);
    printf("   EMCY:");
    for (int i = 0; i < f->len; i++) printf(" %02X", f->data[i]);
    printf("\n");
    CHECK(f->data[0] == 0 && f->data[2] == 0x15 && f->data[3] == 0x05 && f->data[4] == 0x00 && f->data[5] == 0x10
          && f->data[6] == 0x01 && f->data[7] == CO_EM_GENERIC_ERROR && f->data[8] == 0xD4 && f->data[11] == 0xA1
          && f->data[12] == CO_EM_FD_STATUS_OCCURRED);
    CO_errorReset(&em, CO_EM_GENERIC_ERROR, 0);
    CO_EM_process(&em, true, 1000, NULL);
    f = &test_txLog[test_txCount - 1];
    printf("   EMCY:");
    for (int i = 0; i < f->len; i++) printf(" %02X", f->data[i]);
    printf("\n");
    CHECK(f->len == 20 && f->data[4] == 0x00 && f->data[5] == 0x00 && f->data[6] == 0 && f->data[12] == 0);

    printf("%s: %d failures\n", test_failures ? "FAILED" : "PASSED", test_failures);
    return test_failures != 0;
}
