#include <cstdio>

#include <storage/CO_storage.h> //before CANopen.h
#include <CANopen.h>

using namespace std;

int
main() {
    CO_t* CO = NULL; /* CANopen object */

    CO_ReturnError_t err;
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    CO_storage_t storage;
    uint32_t heapMemoryUsed;
    CO_config_t* config_ptr = NULL;

    CO = CO_new(config_ptr, &heapMemoryUsed);
    if (CO == NULL) {
        printf("Error: Can't allocate memory\n");
        return 0;
    } else {
        printf("Allocated %u bytes for CANopen objects\n", heapMemoryUsed);
    }
    return 0;
}