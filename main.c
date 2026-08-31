#include "modbus_u3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <errno.h>

#define PORT 5020
#define MAX_BUF 260

static int read_exact(int fd, uint8_t *buf, size_t len)
{
    size_t n = 0;
    while (n < len) {
        ssize_t r = read(fd, buf + n, len - n);
        if (r <= 0)
            return -1;
        n += r;
    }
    return 0;
}

static int write_exact(int fd, const uint8_t *buf, size_t len)
{
    size_t n = 0;
    while (n < len) {
        ssize_t r = write(fd, buf + n, len - n);
        if (r <= 0)
            return -1;
        n += r;
    }
    return 0;
}

static int handle_modbus_request(modbus_ctx_t *ctx, int client_fd)
{
    uint8_t buf[MAX_BUF];
    int r;
    (void)r; /* silence unused warning if not used further */

    /* Read MBAP header (7 bytes) */






if (read_exact(client_fd, buf, 7) != 0)
    return -1;

uint16_t proto = ((uint16_t)buf[2] << 8) | buf[3];
uint16_t len = ((uint16_t)buf[4] << 8) | buf[5];
uint8_t uid = buf[6];

if (proto != 0)
    return -1;

/* Length is Unit ID plus PDU. Unit ID was already read. */
if (len < 2 || len > (MAX_BUF - 6))
    return -1;

if (read_exact(client_fd, buf + 7, len - 1) != 0)
    return -1;

uint8_t fc = buf[7];
uint8_t *pdu_in = buf + 8;
size_t pdu_in_len = len - 2;











    uint8_t resp[MAX_BUF];
    size_t resp_len = 0;

    /* Build MBAP response header */
    resp[0] = buf[0];
    resp[1] = buf[1];
    resp[2] = 0;
    resp[3] = 0;
    resp[6] = uid;

    int exception = 0;
    uint8_t exception_code = 0;

    switch (fc) {
    case 0x01: /* Read Coils */
    {
        if (pdu_in_len < 4) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        uint16_t start_addr = (pdu_in[0] << 8) | pdu_in[1];
        uint16_t count = (pdu_in[2] << 8) | pdu_in[3];
        if (count == 0 || count > 2000) {
            exception = 1;
            exception_code = 0x02;
            break;
        }

        uint8_t *coil_vals = malloc(count);
        if (!coil_vals) {
            exception = 1;
            exception_code = 0x04;
            break;
        }

        int ok = 0;
        if (mb_read_coils_bulk(ctx, start_addr, count, coil_vals) == 0) {
            ok = 1;
        }

        if (!ok) {
            free(coil_vals);
            exception = 1;
            exception_code = 0x02;
            break;
        }

        uint8_t byte_count = (count + 7) / 8;
        resp[7] = 0x01;
        resp[8] = byte_count;
        memset(resp + 9, 0, byte_count);

        for (uint16_t i = 0; i < count; i++) {
            if (coil_vals[i] & 0x01) {
                resp[9 + (i / 8)] |= (1 << (i % 8));
            }
        }
        resp_len = 9 + byte_count;

        free(coil_vals);
        break;
    }

    case 0x02: /* Read Discrete Inputs */
    {
        if (pdu_in_len < 4) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        uint16_t start_addr = (pdu_in[0] << 8) | pdu_in[1];
        uint16_t count = (pdu_in[2] << 8) | pdu_in[3];
        if (count == 0 || count > 2000) {
            exception = 1;
            exception_code = 0x02;
            break;
        }

        uint8_t *vals = malloc(count);
        if (!vals) {
            exception = 1;
            exception_code = 0x04;
            break;
        }
        int ok = 1;
        for (uint16_t i = 0; i < count; i++) {
            if (mb_read_discrete_input(ctx, start_addr + i, &vals[i]) != 0) {
                ok = 0;
                break;
            }
        }

        if (!ok) {
            free(vals);
            exception = 1;
            exception_code = 0x02;
            break;
        }








        uint8_t byte_count = (count + 7) / 8;
        resp[7] = 0x02;
        resp[8] = byte_count;
        memset(resp + 9, 0, byte_count);

        for (uint16_t i = 0; i < count; i++) {
            if (vals[i] & 0x01) {
                resp[9 + (i / 8)] |= (1 << (i % 8));
            }
        }
        resp_len = 9 + byte_count;
        free(vals);
        break;
    }

    case 0x03: /* Read Holding Registers */
    {
        if (pdu_in_len < 4) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        uint16_t start_addr = (pdu_in[0] << 8) | pdu_in[1];
        uint16_t count = (pdu_in[2] << 8) | pdu_in[3];
        if (count == 0 || count > 125) {
            exception = 1;
            exception_code = 0x02;
            break;
        }

        uint16_t *regs = malloc(count * sizeof(uint16_t));
        if (!regs) {
            exception = 1;
            exception_code = 0x04;
            break;
        }

        int ok = 0;
        if (mb_read_holding_registers_bulk(ctx, start_addr, count, regs) == 0) {
            ok = 1;
        }

        if (!ok) {
            free(regs);
            exception = 1;
            /* Address is valid; the U3 operation failed. */
            exception_code = 0x04;
            break;
        }

        uint8_t byte_count = count * 2;
        resp[7] = 0x03;
        resp[8] = byte_count;
        for (uint16_t i = 0; i < count; i++) {
            resp[9 + 2 * i] = (regs[i] >> 8) & 0xFF;
            resp[10 + 2 * i] = regs[i] & 0xFF;
        }
        resp_len = 9 + byte_count;
        free(regs);
        break;
    }

    case 0x04: /* Read Input Registers */
    {
        if (pdu_in_len < 4) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        uint16_t start_addr = (pdu_in[0] << 8) | pdu_in[1];
        uint16_t count = (pdu_in[2] << 8) | pdu_in[3];
        if (count == 0 || count > 125) {
            exception = 1;
            exception_code = 0x02;
            break;
        }

        uint16_t *regs = malloc(count * sizeof(uint16_t));
        if (!regs) {
            exception = 1;
            exception_code = 0x04;
            break;
        }

        int ok = 1;
        for (uint16_t i = 0; i < count; i++) {
            if (mb_read_input_register(ctx, start_addr + i, &regs[i]) != 0) {
                ok = 0;
                break;
            }
        }

        if (!ok) {
            free(regs);
            exception = 1;
            /* Address is valid; the U3 operation failed. */
            exception_code = 0x04;
            break;
        }

        uint8_t byte_count = count * 2;
        resp[7] = 0x04;
        resp[8] = byte_count;
        for (uint16_t i = 0; i < count; i++) {
            resp[9 + 2 * i] = (regs[i] >> 8) & 0xFF;
            resp[10 + 2 * i] = regs[i] & 0xFF;
        }
        resp_len = 9 + byte_count;
        free(regs);
        break;
    }

    case 0x05: /* Write Single Coil */
    {
        if (pdu_in_len < 4) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        uint16_t addr = (pdu_in[0] << 8) | pdu_in[1];
        uint16_t value = (pdu_in[2] << 8) | pdu_in[3];

        if (value != 0x0000 && value != 0xFF00) {
            exception = 1;
            exception_code = 0x03;
            break;
        }

        if (mb_write_coil(ctx, addr, value ? 1 : 0) != 0) {
            exception = 1;
            exception_code = 0x02;
            break;
        }

        resp[7] = 0x05;
        memcpy(resp + 8, pdu_in, 4);
        resp_len = 12;
        break;
    }

    case 0x06: /* Write Single Register */
    {
        if (pdu_in_len < 4) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        uint16_t addr = (pdu_in[0] << 8) | pdu_in[1];
        uint16_t value = (pdu_in[2] << 8) | pdu_in[3];

        if (mb_write_holding_register(ctx, addr, value) != 0) {
            exception = 1;
            exception_code = 0x02;
            break;
        }

        resp[7] = 0x06;
        memcpy(resp + 8, pdu_in, 4);
        resp_len = 12;
        break;
    }

    case 0x0F: /* Write Multiple Coils */
    {
        if (pdu_in_len < 5) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        uint16_t start_addr = (pdu_in[0] << 8) | pdu_in[1];
        uint16_t count = (pdu_in[2] << 8) | pdu_in[3];
        uint8_t byte_count = pdu_in[4];

        if (count == 0 || count > 1968 || byte_count != (count + 7) / 8) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        if (5 + byte_count != pdu_in_len) {
            exception = 1;
            exception_code = 0x02;
            break;
        }

        uint16_t *values = malloc(count * sizeof(uint16_t));
        if (!values) {
            exception = 1;
            exception_code = 0x04;
            break;
        }

        for (uint16_t i = 0; i < count; i++) {
            uint8_t b = pdu_in[5 + (i / 8)];
            values[i] = (b & (1 << (i % 8))) ? 1 : 0;
        }

        if (mb_write_coils_bulk(ctx, start_addr, count, values) != 0) {
            free(values);
            exception = 1;
            exception_code = 0x02;
            break;
        }

        resp[7] = 0x0F;
        memcpy(resp + 8, pdu_in, 4);
        resp_len = 12;
        free(values);
        break;
    }

    case 0x10: /* Write Multiple Registers */
    {
        if (pdu_in_len < 5) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        uint16_t start_addr = (pdu_in[0] << 8) | pdu_in[1];
        uint16_t count = (pdu_in[2] << 8) | pdu_in[3];
        uint8_t byte_count = pdu_in[4];

        if (count == 0 || count > 123 || byte_count != count * 2) {
            exception = 1;
            exception_code = 0x02;
            break;
        }
        if (5 + byte_count != pdu_in_len) {
            exception = 1;
            exception_code = 0x02;
            break;
        }

        uint16_t *values = malloc(count * sizeof(uint16_t));
        if (!values) {
            exception = 1;
            exception_code = 0x04;
            break;
        }

        for (uint16_t i = 0; i < count; i++) {
            values[i] = (pdu_in[5 + 2 * i] << 8) | pdu_in[6 + 2 * i];
        }

        if (mb_write_holding_registers_bulk(ctx, start_addr, count, values) != 0) {
            free(values);
            exception = 1;
            exception_code = 0x02;
            break;
        }

        resp[7] = 0x10;
        memcpy(resp + 8, pdu_in, 4);
        resp_len = 12;
        free(values);
        break;
    }

    default:
        exception = 1;
        exception_code = 0x01;
        break;
    }

    if (exception) {
        resp[7] = fc | 0x80;
        resp[8] = exception_code;
        resp_len = 9;
    }

    /* Set length field */
    uint16_t resp_data_len = (uint16_t)(resp_len - 6);
    resp[4] = (resp_data_len >> 8) & 0xFF;
    resp[5] = resp_data_len & 0xFF;

    if (write_exact(client_fd, resp, resp_len) != 0)
        return -1;

    return 0;
}

int main(void)
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    modbus_ctx_t ctx;

    if (modbus_init(&ctx) != 0) {
        fprintf(stderr, "Failed to initialize Modbus/U3 context\n");
        return 1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        modbus_close(&ctx);
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        modbus_close(&ctx);
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        modbus_close(&ctx);
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        modbus_close(&ctx);
        return 1;
    }

    printf("Modbus TCP server listening on port %d\n", PORT);

    for (;;) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Client connected from %s\n", client_ip);

        while (handle_modbus_request(&ctx, client_fd) == 0) {
            /* loop handling multiple requests on same connection */
        }

        close(client_fd);
        printf("Client disconnected\n");
    }

    close(server_fd);
    modbus_close(&ctx);
    return 0;
}


