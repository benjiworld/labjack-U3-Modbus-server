#ifndef MODBUS_U3_H
#define MODBUS_U3_H
#include "u3.h"
#include <stddef.h>
#include <stdint.h>
/* DIELTECH extension, absent from the cited UD table; not LabJack-reserved. */
#define CUSTOM_BASE 45000U
#define CUSTOM_COMMAND 45020U
#define APPLY_MAGIC 0xA55AU
typedef struct {
    HANDLE handle;
    uint16_t stage[13];
    uint16_t apply_status, last_error, loaded, writes_done;
    uint16_t transaction;
} modbus_ctx_t;
int modbus_init(modbus_ctx_t *ctx);
void modbus_close(modbus_ctx_t *ctx);
/* Return Modbus exception code, or zero. output includes function byte. */
int mb_request(modbus_ctx_t *ctx, uint8_t unit, const uint8_t *pdu,
               size_t size, uint8_t *output, size_t *output_size);
#endif
