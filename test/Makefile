# Unit tests for CANopenNode, run on host computer with mock CAN driver.
#
# Usage: make        - build and run all tests
#        make clean
#
# Each test is compiled and run in several stack configurations (classic CAN, CAN FD, bitwise PDO mapping, ...),
# with address and undefined behavior sanitizers enabled.

CANOPEN_SRC = ..
BUILD = build

CC ?= gcc
CFLAGS = -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
         -fsanitize=address,undefined -fno-sanitize-recover=all \
         -I. -I$(CANOPEN_SRC) -DCO_CONFIG_EM=0x01

DRIVER = test_driver.c $(CANOPEN_SRC)/301/CO_ODinterface.c $(CANOPEN_SRC)/301/CO_Emergency.c
PDO_SRC = $(DRIVER) test_pdo.c $(CANOPEN_SRC)/301/CO_PDO.c $(CANOPEN_SRC)/301/CO_SYNC.c
USDO_SRC = $(DRIVER) test_usdo.c $(CANOPEN_SRC)/301/CO_USDOserver.c $(CANOPEN_SRC)/301/CO_USDOclient.c

FD = -DCO_CONFIG_CAN_FD=1 -DCO_PDO_MAX_MAPPED_ENTRIES=16
PDO_BITWISE = -DCO_CONFIG_PDO=0x407F
PDO_NO_DYNAMIC = -DCO_CONFIG_PDO=0x3F
USDO = -DCO_CONFIG_CAN_FD=1 -DCO_CONFIG_USDO=0x17

# test name : extra compiler flags
TESTS = \
	pdo_classic \
	pdo_classic_bitwise \
	pdo_fd \
	pdo_fd_bitwise \
	pdo_fd_padding \
	pdo_fd_no_dynamic \
	usdo \
	usdo_small_buffer \
	usdo_large_buffer

FLAGS_pdo_classic =
FLAGS_pdo_classic_bitwise = $(PDO_BITWISE)
FLAGS_pdo_fd = $(FD)
FLAGS_pdo_fd_bitwise = $(FD) $(PDO_BITWISE)
FLAGS_pdo_fd_padding = $(FD) -DCO_CAN_FD_PADDING_BYTE=0xCC
FLAGS_pdo_fd_no_dynamic = $(FD) $(PDO_NO_DYNAMIC)
FLAGS_usdo = $(USDO)
FLAGS_usdo_small_buffer = $(USDO) -DCO_CONFIG_USDO_SRV_BUFFER_SIZE=64
FLAGS_usdo_large_buffer = $(USDO) -DCO_CONFIG_USDO_SRV_BUFFER_SIZE=300 -DCO_CAN_FD_PADDING_BYTE=0xCC

SRC_pdo = $(PDO_SRC)
SRC_usdo = $(USDO_SRC)

.PHONY: all clean

all: $(TESTS:%=$(BUILD)/%)
	@failed=0; \
	for t in $(TESTS); do \
		if $(BUILD)/$$t > $(BUILD)/$$t.log 2>&1; then \
			echo "PASS  $$t"; \
		else \
			echo "FAIL  $$t"; cat $(BUILD)/$$t.log; failed=1; \
		fi; \
	done; \
	exit $$failed

$(BUILD)/%: test_*.c test_driver.h CO_driver_target.h $(CANOPEN_SRC)/301/*.c $(CANOPEN_SRC)/301/*.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(FLAGS_$*) $(SRC_$(firstword $(subst _, ,$*))) -o $@

clean:
	rm -rf $(BUILD)
