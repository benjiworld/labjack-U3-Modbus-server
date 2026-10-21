/*
 * main.c
 *
 * DIELTECH srl
 * LabJack U3 native UD Modbus TCP gateway - map v2
 *
 * TCP port: 5020
 *
 * Connection behavior:
 * - Idle connections remain open.
 * - Partial messages must complete within the socket timeout.
 * - USB/backend errors are returned as Modbus exceptions.
 * - One client is served at a time.
 */

#include "modbus_u3.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define PORT                 5020
#define SOCKET_TIMEOUT_SEC   10
#define LISTEN_BACKLOG       5

#define MBAP_HEADER_SIZE     7U
#define MAX_PDU_SIZE         253U
#define MAX_ADU_SIZE         260U

enum {
    IO_ERROR = -1,
    IO_OK = 0,
    IO_IDLE = 1
};

/*
 * Transfer exactly len bytes.
 *
 * sending:
 *   0 = receive
 *   1 = send
 *
 * allow_idle:
 *   Permit an idle receive timeout before the first byte.
 *   Enable this only while waiting for a new MBAP header.
 *
 * Returns:
 *   IO_OK    complete transfer
 *   IO_IDLE  receive timeout before any byte, when permitted
 *   IO_ERROR connection closed, transfer error or partial timeout
 */
static int transfer_exact(
    int fd,
    uint8_t *buffer,
    size_t len,
    int sending,
    int allow_idle)
{
    size_t completed = 0;

    while (completed < len) {
        ssize_t transferred;

        if (sending) {
            transferred = send(
                fd,
                buffer + completed,
                len - completed,
                0);
        } else {
            transferred = recv(
                fd,
                buffer + completed,
                len - completed,
                0);
        }

        if (transferred < 0) {
            int error = errno;

            if (error == EINTR)
                continue;

            if (error == EAGAIN || error == EWOULDBLOCK) {
                /*
                 * A timeout before a new request starts is normal
                 * inactivity, not a disconnected client.
                 */
                if (!sending && allow_idle && completed == 0)
                    return IO_IDLE;

                fprintf(
                    stderr,
                    "TCP %s timeout: %zu/%zu bytes\n",
                    sending ? "send" : "receive",
                    completed,
                    len);
            } else {
                fprintf(
                    stderr,
                    "TCP %s failed: %s (%d), %zu/%zu bytes\n",
                    sending ? "send" : "receive",
                    strerror(error),
                    error,
                    completed,
                    len);
            }

            return IO_ERROR;
        }

        if (transferred == 0) {
            fprintf(
                stderr,
                "TCP %s: connection closed, %zu/%zu bytes\n",
                sending ? "send" : "receive",
                completed,
                len);

            return IO_ERROR;
        }

        completed += (size_t)transferred;
    }

    return IO_OK;
}

static int configure_client_socket(int client_fd)
{
    struct timeval timeout;

    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = SOCKET_TIMEOUT_SEC;

    if (setsockopt(
            client_fd,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO");
        return -1;
    }

    if (setsockopt(
            client_fd,
            SOL_SOCKET,
            SO_SNDTIMEO,
            &timeout,
            sizeof(timeout)) < 0) {
        perror("setsockopt SO_SNDTIMEO");
        return -1;
    }

    return 0;
}

static void print_banner(void)
{
    printf(
        "\n"
        "============================================================\n"
        " DIELTECH srl\n"
        " LabJack U3 native UD Modbus TCP gateway\n"
        " Register map: v2\n"
        "============================================================\n"
        " TCP port                  : %d\n"
        " Idle connection timeout   : connection remains open\n"
        " Partial transfer timeout  : %d seconds per socket operation\n"
        " Client handling           : one client at a time\n"
        "============================================================\n"
        "\n",
        PORT,
        SOCKET_TIMEOUT_SEC);

    printf(
        "Modbus TCP server listening on port %d\n",
        PORT);

    fflush(stdout);
}

static void serve_client(
    modbus_ctx_t *ctx,
    int client_fd)
{
    uint8_t request[MAX_ADU_SIZE];
    uint8_t response[MAX_ADU_SIZE];

    for (;;) {
        size_t mbap_length;
        size_t request_pdu_size;
        size_t response_pdu_size = 0;
        size_t response_length;
        int result;
        int exception;

        /*
         * A new request starts with the seven-byte MBAP header.
         * Idle timeouts here do not close the connection.
         */
        result = transfer_exact(
            client_fd,
            request,
            MBAP_HEADER_SIZE,
            0,
            1);

        if (result == IO_IDLE)
            continue;

        if (result != IO_OK)
            break;

        /*
         * Protocol Identifier must be zero for Modbus TCP.
         */
        if (request[2] != 0 || request[3] != 0) {
            fprintf(
                stderr,
                "Invalid MBAP protocol identifier: 0x%02X%02X\n",
                (unsigned)request[2],
                (unsigned)request[3]);

            break;
        }

        /*
         * MBAP Length counts Unit ID + PDU.
         * Unit ID has already been received in the header.
         */
        mbap_length =
            ((size_t)request[4] << 8) |
            (size_t)request[5];

        if (mbap_length < 2 ||
            mbap_length > MAX_PDU_SIZE + 1U) {
            fprintf(
                stderr,
                "Invalid MBAP length: %zu\n",
                mbap_length);

            break;
        }

        request_pdu_size = mbap_length - 1U;

        /*
         * Once the header has arrived, the remaining PDU must
         * arrive within the socket timeout. This is not idle time.
         */
        result = transfer_exact(
            client_fd,
            request + MBAP_HEADER_SIZE,
            request_pdu_size,
            0,
            0);

        if (result != IO_OK)
            break;

        /*
         * Echo transaction ID, protocol ID and Unit ID.
         * Response length is filled in after backend processing.
         */
        memcpy(response, request, MBAP_HEADER_SIZE);

        exception = mb_request(
            ctx,
            request[6],
            request + MBAP_HEADER_SIZE,
            request_pdu_size,
            response + MBAP_HEADER_SIZE,
            &response_pdu_size);

        if (exception != 0) {
            /*
             * Modbus exception PDU:
             *   function | 0x80
             *   exception code
             */
            response[7] = (uint8_t)(request[7] | 0x80U);

            response[8] =
                (exception > 0 && exception <= 255)
                    ? (uint8_t)exception
                    : (uint8_t)0x04;

            response_pdu_size = 2;

            fprintf(
                stderr,
                "Modbus exception: unit=%u function=0x%02X "
                "code=0x%02X\n",
                (unsigned)request[6],
                (unsigned)request[7],
                (unsigned)response[8]);
        } else if (
            response_pdu_size == 0 ||
            response_pdu_size > MAX_PDU_SIZE) {
            /*
             * Defensive check for an invalid backend result.
             */
            fprintf(
                stderr,
                "Invalid backend response size: %zu\n",
                response_pdu_size);

            response[7] = (uint8_t)(request[7] | 0x80U);
            response[8] = 0x04;
            response_pdu_size = 2;
        }

        response_length = response_pdu_size + 1U;

        response[4] =
            (uint8_t)(response_length >> 8);

        response[5] =
            (uint8_t)response_length;

        result = transfer_exact(
            client_fd,
            response,
            MBAP_HEADER_SIZE + response_pdu_size,
            1,
            0);

        if (result != IO_OK)
            break;
    }
}

int main(void)
{
    modbus_ctx_t ctx;
    struct sockaddr_in server_address;
    int listen_fd;
    int reuse_address = 1;
    int exit_status = 0;

    /*
     * Prevent process termination when sending to a disconnected
     * client. send() will return an error instead.
     */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal SIGPIPE");
        return 1;
    }

    if (modbus_init(&ctx) != 0) {
        fprintf(
            stderr,
            "Cannot initialize LabJack U3 backend\n");

        return 1;
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd < 0) {
        perror("socket");
        modbus_close(&ctx);
        return 1;
    }

    if (setsockopt(
            listen_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse_address,
            sizeof(reuse_address)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(listen_fd);
        modbus_close(&ctx);
        return 1;
    }

    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons((uint16_t)PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
            listen_fd,
            (struct sockaddr *)&server_address,
            sizeof(server_address)) < 0) {
        perror("bind");
        close(listen_fd);
        modbus_close(&ctx);
        return 1;
    }

    if (listen(listen_fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(listen_fd);
        modbus_close(&ctx);
        return 1;
    }

    print_banner();

    for (;;) {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);
        char client_ip[INET_ADDRSTRLEN];
        const char *ip;
        int client_fd;

        client_fd = accept(
            listen_fd,
            (struct sockaddr *)&client_address,
            &client_address_size);

        if (client_fd < 0) {
            if (errno == EINTR)
                continue;

            perror("accept");
            exit_status = 1;
            break;
        }

        ip = inet_ntop(
            AF_INET,
            &client_address.sin_addr,
            client_ip,
            sizeof(client_ip));

        if (ip == NULL)
            ip = "unknown";

        printf(
            "Client connected from %s:%u\n",
            ip,
            (unsigned)ntohs(client_address.sin_port));

        fflush(stdout);

        if (configure_client_socket(client_fd) == 0)
            serve_client(&ctx, client_fd);

        close(client_fd);

        printf("Client disconnected\n");
        fflush(stdout);
    }

    close(listen_fd);
    modbus_close(&ctx);

    return exit_status;
}
