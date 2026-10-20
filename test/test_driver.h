/*
 * Mock CAN driver for the CANopenNode unit tests, see test_driver.c.
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

#ifndef TEST_DRIVER_H
#define TEST_DRIVER_H
#include "301/CO_driver.h"
#define TEST_TX_LOG 256
typedef struct {
    uint16_t ident;
    uint8_t len;
    uint8_t data[64];
} test_frame_t;
extern test_frame_t test_txLog[TEST_TX_LOG];
extern int test_txCount;
extern int test_failures;
extern bool_t test_busEnabled;
void test_receive(CO_CANmodule_t* CANmodule, uint16_t ident, const uint8_t* data, uint8_t len);
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                     \
            test_failures++;                                                                                           \
        }                                                                                                              \
    } while (0)
#endif
