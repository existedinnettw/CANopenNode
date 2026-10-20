/*
 * Mock CAN driver for the CANopenNode unit tests.
 *
 * It implements functions from CO_driver.h without hardware:
 * - every frame sent with CO_CANsend() is recorded in test_txLog[],
 * - test_receive() injects a received frame: it searches rxArray for matching CAN-ID (as hardware filter would do)
 *   and calls the CANrx_callback of the CANopenNode object,
 * - if test_busEnabled is true, CO_CANsend() also delivers the frame to all other CAN modules (simulated CAN bus),
 *   so two CANopen nodes (e.g. USDO client and server) can talk to each other.
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
#include "301/CO_driver.h"
#include "test_driver.h"
#include <stdio.h>

test_frame_t test_txLog[TEST_TX_LOG];
int test_txCount;
bool_t test_busEnabled;
static CO_CANmodule_t* busModules[8];
static int busModulesCount;

void CO_CANsetConfigurationMode(void* CANptr) { (void)CANptr; }
void CO_CANsetNormalMode(CO_CANmodule_t* CANmodule) { CANmodule->CANnormal = true; }

CO_ReturnError_t
CO_CANmodule_init(CO_CANmodule_t* CANmodule, void* CANptr, CO_CANrx_t rxArray[], uint16_t rxSize, CO_CANtx_t txArray[],
                  uint16_t txSize, uint16_t CANbitRate) {
    (void)CANbitRate;
    memset(CANmodule, 0, sizeof(*CANmodule));
    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    int found = 0;
    for (int i = 0; i < busModulesCount; i++) {
        if (busModules[i] == CANmodule) {
            found = 1;
        }
    }
    if (!found && busModulesCount < 8) {
        busModules[busModulesCount++] = CANmodule;
    }
    for (uint16_t i = 0; i < rxSize; i++) {
        rxArray[i].ident = 0;
        rxArray[i].mask = 0xFFFF;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
    }
    return CO_ERROR_NO;
}

void CO_CANmodule_disable(CO_CANmodule_t* CANmodule) { (void)CANmodule; }

CO_ReturnError_t
CO_CANrxBufferInit(CO_CANmodule_t* CANmodule, uint16_t index, uint16_t ident, uint16_t mask, bool_t rtr, void* object,
                   void (*CANrx_callback)(void* object, void* message)) {
    if (CANmodule == NULL || index >= CANmodule->rxSize || object == NULL || CANrx_callback == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
    CO_CANrx_t* b = &CANmodule->rxArray[index];
    b->object = object;
    b->CANrx_callback = CANrx_callback;
    b->ident = (ident & 0x7FF) | (rtr ? 0x800 : 0);
    b->mask = (mask & 0x7FF) | 0x800;
    return CO_ERROR_NO;
}

CO_CANtx_t*
CO_CANtxBufferInit(CO_CANmodule_t* CANmodule, uint16_t index, uint16_t ident, bool_t rtr, uint8_t noOfBytes,
                   bool_t syncFlag) {
    if (CANmodule == NULL || index >= CANmodule->txSize) {
        return NULL;
    }
    if (noOfBytes > CO_CAN_DATA_MAX_LEN || noOfBytes != CO_CANframeLength(noOfBytes)) {
        printf("FAIL: invalid tx length %u for ident 0x%03X\n", noOfBytes, ident);
        test_failures++;
    }
    CO_CANtx_t* b = &CANmodule->txArray[index];
    b->ident = ident & 0x7FF;
    b->DLC = noOfBytes;
    (void)rtr;
    b->bufferFull = false;
    b->syncFlag = syncFlag;
    return b;
}

CO_ReturnError_t
CO_CANsend(CO_CANmodule_t* CANmodule, CO_CANtx_t* buffer) {
    (void)CANmodule;
    if (test_txCount < TEST_TX_LOG) {
        test_frame_t* f = &test_txLog[test_txCount];
        f->ident = (uint16_t)buffer->ident;
        f->len = buffer->DLC;
        memcpy(f->data, buffer->data, buffer->DLC);
    }
    test_txCount++;
    if (test_busEnabled) {
        for (int i = 0; i < busModulesCount; i++) {
            if (busModules[i] != CANmodule) {
                test_receive(busModules[i], (uint16_t)buffer->ident, buffer->data, buffer->DLC);
            }
        }
    }
    return CO_ERROR_NO;
}

void CO_CANclearPendingSyncPDOs(CO_CANmodule_t* CANmodule) { (void)CANmodule; }
void CO_CANmodule_process(CO_CANmodule_t* CANmodule) { (void)CANmodule; }

int test_failures;

void
test_receive(CO_CANmodule_t* CANmodule, uint16_t ident, const uint8_t* data, uint8_t len) {
    test_rxMsg_t msg;
    memset(&msg, 0xEE, sizeof(msg));
    msg.ident = ident;
    msg.DLC = len;
    memcpy(msg.data, data, len);
    for (uint16_t i = 0; i < CANmodule->rxSize; i++) {
        CO_CANrx_t* b = &CANmodule->rxArray[i];
        if (b->CANrx_callback != NULL && ((ident ^ b->ident) & b->mask) == 0) {
            b->CANrx_callback(b->object, &msg);
            return;
        }
    }
}
