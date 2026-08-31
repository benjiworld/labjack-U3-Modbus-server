#include "modbus_u3.h"
#include "u3.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define LJ_tmPWM16                   0
#define LJ_tmPWM8                    1
#define LJ_tmPERIOD_IN               2
#define LJ_tmDUTYCYCLE               4
#define LJ_tmFREQ_OUT                7

static void set_last_error(modbus_ctx_t *ctx, long err);
static void float_to_regs(float f, uint16_t *regs);
static float regs_to_float(const uint16_t *regs);
static void u32_to_regs(uint32_t val, uint16_t *regs);

static int open_u3(modbus_ctx_t *ctx)
{
    HANDLE h = NULL;
    long r;
    int i;

    r = eOpenU3(&h);
    if (r != 0 || h == NULL) {
        fprintf(stderr, "eOpenU3 failed: %ld\n", r);
        return -1;
    }

    ctx->u3_handle = h;
    ctx->u3_opened = 1;

    r = getCalibrationInfo(h, &ctx->calibration_info);
    if (r != 0) {
        fprintf(stderr, "getCalibrationInfo failed: result=%ld\n", r);
        eClose(h);
        ctx->u3_handle = NULL;
        ctx->u3_opened = 0;
        return -1;
    }

    ctx->num_ain = 16;
    ctx->num_dio = 20;

    for(i=0; i<2; i++) {
        ctx->timerEnabled[i] = 0;
        ctx->counterEnabled[i] = 0;
        ctx->timerModes[i] = LJ_tmPWM8;
        ctx->timerValues[i] = 16384;
        ctx->counterValues[i] = 0;
    }

    ctx->timerClockBase = 26;
    ctx->timerClockDivisor = 48;
    ctx->dacSetpoint[0] = 0.0;
    ctx->dacSetpoint[1] = 0.0;

    for (i = 0; i < 16; i++) {
        ctx->ainScale[i] = 10000;
        ctx->ainOffset[i] = 0;
    }
    ctx->counterScale = 10000;
    ctx->counterOffset = 0;

    ctx->lastError = 0;
    ctx->startTime = (long)time(NULL);

    return 0;
}

int modbus_init(modbus_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    return open_u3(ctx);
}

void modbus_close(modbus_ctx_t *ctx)
{
    if (!ctx->u3_opened || ctx->u3_handle == NULL)
        return;
    eClose(ctx->u3_handle);
    ctx->u3_handle = NULL;
    ctx->u3_opened = 0;
}

static void set_last_error(modbus_ctx_t *ctx, long err)
{
    ctx->lastError = err;
}

static void float_to_regs(float f, uint16_t *regs) {
    uint32_t val;
    memcpy(&val, &f, sizeof(val));
    regs[0] = (val >> 16) & 0xFFFF;
    regs[1] = val & 0xFFFF;
}

static float regs_to_float(const uint16_t *regs) {
    uint32_t val = ((uint32_t)regs[0] << 16) | regs[1];
    float f;
    memcpy(&f, &val, sizeof(f));
    return f;
}

static void u32_to_regs(uint32_t val, uint16_t *regs) {
    regs[0] = (val >> 16) & 0xFFFF;
    regs[1] = val & 0xFFFF;
}

int mb_read_coil(modbus_ctx_t *ctx, uint16_t addr, uint8_t *value) { *value = 0; return 0; }
int mb_write_coil(modbus_ctx_t *ctx, uint16_t addr, uint16_t value) { return 0; }
int mb_read_discrete_input(modbus_ctx_t *ctx, uint16_t addr, uint8_t *value) { *value = 0; return 0; }
int mb_read_coils_bulk(modbus_ctx_t *ctx, uint16_t addr, uint16_t count, uint8_t *values) {
    memset(values, 0, (count + 7) / 8); return 0;
}
int mb_write_coils_bulk(modbus_ctx_t *ctx, uint16_t addr, uint16_t count, const uint16_t *values) { return 0; }

int mb_read_holding_register(modbus_ctx_t *ctx, uint16_t addr, uint16_t *value) {
    return mb_read_holding_registers_bulk(ctx, addr, 1, value);
}
int mb_write_holding_register(modbus_ctx_t *ctx, uint16_t addr, uint16_t value) {
    return mb_write_holding_registers_bulk(ctx, addr, 1, &value);
}
int mb_read_input_register(modbus_ctx_t *ctx, uint16_t addr, uint16_t *value) {
    return mb_read_holding_registers_bulk(ctx, addr, 1, value);
}

int mb_read_holding_registers_bulk(modbus_ctx_t *ctx, uint16_t addr, uint16_t count, uint16_t *values)
{
    int i;
    long r;
    uint16_t val;

    for (i = 0; i < count; i++) {
        uint16_t reg_addr = addr + i;
        val = 0;

        if (reg_addr >= 0 && reg_addr < 32) {
            long ch = reg_addr / 2;
            double volts = 0.0;
            long dac1Enable = 0; // Required by eAIN signature

            if (reg_addr % 2 == 0) {
                // eAIN signature: 
                // long eAIN(HANDLE Handle, u3CalibrationInfo *CalibrationInfo, long ConfigIO, long *DAC1Enable, long ChannelP, long ChannelN, double *Voltage, long Range, long Resolution, long Settling, long Binary, long Reserved1, long Reserved2)
                r = eAIN(ctx->u3_handle, &ctx->calibration_info, 1, &dac1Enable, ch, 31, &volts, 0, 0, 0, 0, 0, 0);
                if (r != 0) set_last_error(ctx, r);

                uint16_t r_buf[2];
                float_to_regs((float)volts, r_buf);
                val = r_buf[0];
            } else {
                r = eAIN(ctx->u3_handle, &ctx->calibration_info, 1, &dac1Enable, ch, 31, &volts, 0, 0, 0, 0, 0, 0);
                uint16_t r_buf[2];
                float_to_regs((float)volts, r_buf);
                val = r_buf[1];
            }
        }
        else if (reg_addr >= 5000 && reg_addr < 5004) {
            uint16_t ch = (reg_addr - 5000) / 2;
            uint16_t r_buf[2];
            float_to_regs((float)ctx->dacSetpoint[ch], r_buf);
            val = (reg_addr % 2 == 0) ? r_buf[0] : r_buf[1];
        }
        else if (reg_addr >= 6000 && reg_addr < 6020) {
            long ch = reg_addr - 6000;
            long state = 0;
            r = eDI(ctx->u3_handle, 1, ch, &state);
            if (r != 0) set_last_error(ctx, r);
            val = (uint16_t)state;
        }
        else if (reg_addr >= 7300 && reg_addr < 7304) {
            uint16_t ch = (reg_addr - 7300) / 2;
            uint16_t r_buf[2];
            u32_to_regs((uint32_t)ctx->counterValues[ch], r_buf);
            val = (reg_addr % 2 == 0) ? r_buf[0] : r_buf[1];
        }

        values[i] = val;
    }
    return 0;
}

int mb_write_holding_registers_bulk(modbus_ctx_t *ctx, uint16_t addr, uint16_t count, const uint16_t *values)
{
    int i;
    long r;
    uint16_t val;

    for (i = 0; i < count; i++) {
        val = values[i];
        uint16_t reg_addr = addr + i;

        if (reg_addr >= 5000 && reg_addr < 5004) {
            if (reg_addr % 2 == 1 && i > 0) {
                long ch = (reg_addr - 5000) / 2;
                uint16_t r_buf[2];
                r_buf[0] = values[i-1];
                r_buf[1] = val;

                float volts = regs_to_float(r_buf);
                ctx->dacSetpoint[ch] = volts;

                // eDAC signature: long eDAC(HANDLE Handle, u3CalibrationInfo *CalibrationInfo, long ConfigIO, long Channel, double Voltage, long Binary, long Reserved1, long Reserved2)
                r = eDAC(ctx->u3_handle, &ctx->calibration_info, 1, ch, (double)volts, 0, 0, 0);
                if (r != 0) set_last_error(ctx, r);
            }
        }
        else if (reg_addr >= 6000 && reg_addr < 6020) {
            long ch = reg_addr - 6000;
            long state = (val != 0) ? 1 : 0;
            r = eDO(ctx->u3_handle, 1, ch, state);
            if (r != 0) set_last_error(ctx, r);
        }
    }
    return 0;
}
