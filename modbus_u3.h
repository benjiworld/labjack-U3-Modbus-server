#ifndef MODBUS_U3_H
#define MODBUS_U3_H

#include <stdint.h>
#include "u3.h"

/*
 * Local compatibility wrappers implemented in u3_easy_wrap.c.
 * The installed U3 header does not declare these easy functions.
 */
long eOpenU3(HANDLE *h);
long eClose(HANDLE h);

long eGet(HANDLE h,
          long ioType,
          long channel,
          long x1,
          long *x2);

long ePut(HANDLE h,
          long ioType,
          long channel,
          long x1,
          long *x2);




typedef struct {
    HANDLE u3_handle;
    u3CalibrationInfo calibration_info;

    int u3_opened;
    int num_ain;
    int num_dio;

    /* Timer/counter state */
    long timerEnabled[2];
    long counterEnabled[2];
    long timerModes[2];
    double timerValues[2];
    double counterValues[2];

    /* Timer clock configuration */
    long timerClockBase;
    long timerClockDivisor;

    /* DAC setpoints, in volts */
    double dacSetpoint[2];

    /* AIN scaling: 10000 means scale 1.0 */
    long ainScale[16];
    long ainOffset[16];

    /* Counter scaling */
    long counterScale;
    long counterOffset;

    /* Diagnostics */
    long lastError;
    long startTime;
} modbus_ctx_t;

/* Initialization and shutdown */
int modbus_init(modbus_ctx_t *ctx);
void modbus_close(modbus_ctx_t *ctx);

/* Coils */
int mb_read_coil(modbus_ctx_t *ctx,
                 uint16_t addr,
                 uint8_t *value);

int mb_write_coil(modbus_ctx_t *ctx,
                  uint16_t addr,
                  uint16_t value);

/* Discrete inputs */
int mb_read_discrete_input(modbus_ctx_t *ctx,
                           uint16_t addr,
                           uint8_t *value);

/* Holding registers */
int mb_read_holding_register(modbus_ctx_t *ctx,
                             uint16_t addr,
                             uint16_t *value);

int mb_write_holding_register(modbus_ctx_t *ctx,
                              uint16_t addr,
                              uint16_t value);

/* Input registers */
int mb_read_input_register(modbus_ctx_t *ctx,
                           uint16_t addr,
                           uint16_t *value);

/* Bulk helpers */
int mb_read_coils_bulk(modbus_ctx_t *ctx,
                       uint16_t addr,
                       uint16_t count,
                       uint8_t *values);

int mb_write_coils_bulk(modbus_ctx_t *ctx,
                        uint16_t addr,
                        uint16_t count,
                        const uint16_t *values);

int mb_read_holding_registers_bulk(modbus_ctx_t *ctx,
                                   uint16_t addr,
                                   uint16_t count,
                                   uint16_t *values);

int mb_write_holding_registers_bulk(modbus_ctx_t *ctx,
                                    uint16_t addr,
                                    uint16_t count,
                                    const uint16_t *values);

#endif /* MODBUS_U3_H */

