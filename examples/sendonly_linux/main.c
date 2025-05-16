#include "rdlc.h"
#include "serial_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TX_MSG_MAX_SIZE        3
#define TX_MSG_MAX_ESCAPE_SIZE 3
#define TX_MAX_BUF_SIZE        RDLC_GET_FRAME_SIZE(TX_MSG_MAX_SIZE,TX_MSG_MAX_ESCAPE_SIZE)
#define SRC_PORT               0x01
#define DST_PORT               0x02

// ========== 回调函数 ==========
int onParsed(Rdlc_t handle, RdlcAddr_t addr, const uint8_t *payload, uint16_t len) {
    printf("[RECV] From %02X -> To %02X | Len: %u | Data: ", addr.srcAddr, addr.dstAddr, len);
    for (int i = 0; i < len; ++i)
        printf("%02X ", payload[i]);
    printf("\n");
    return 0;
}

int onError(Rdlc_t handle, int err) {
    fprintf(stderr, "[ERROR] Code: %d\n", err);
    return 0;
}

// ========== 日志接口 ==========
int portPrintf(RdlcLogLevel_t level, const char *fmt, va_list args) {
    vprintf(fmt, args);
    return 0;
}

// ========== 主程序 ==========
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <serial_device> <baudrate>\n", argv[0]);
        return 1;
    }

    const char *serial_device = argv[1];
    int baudrate = atoi(argv[2]);

    int fd = open_serial(serial_device, baudrate);
    if (fd < 0) {
        perror("open_serial");
        return 1;
    }

    static RdlcStaticHandle_t handle;
    static uint8_t rxBuf[RX_BUF_SIZE];

    RdlcConfig_t config = {
        .msgMaxSize = TX_MSG_MAX_SIZE,
        .msgMaxEscapeSize = TX_MSG_MAX_ESCAPE_SIZE,
        .cbParsed = onParsed,
        .cbError = onError
    };

    RdlcPort_t port = {
        .portMalloc = malloc,
        .portFree = free,
        .portPrintf = portPrintf
    };

    Rdlc_t proto = xRdlcCreateStatic(&config, &port, &handle, rxBuf, sizeof(rxBuf));
    if (!proto) {
        fprintf(stderr, "Failed to create RDLC instance\n");
        return 1;
    }

    uint8_t readBuf[64];
    uint8_t frameBuf[TX_BUF_SIZE];

    printf("[INFO] Listening on %s @ %d baud...\n", serial_device, baudrate);

    while (1) {
        uint8_t testPayload[] = {0x11, 0x22, 0x33};
        RdlcAddr_t addr = {.srcAddr = SRC_PORT, .dstAddr = DST_PORT};
        int len = xRdlcWriteBytes(proto, addr, testPayload, sizeof(testPayload), frameBuf, sizeof(frameBuf));
        if (len > 0) {
            write(fd, frameBuf, len);
            printf("[SEND] %d bytes sent.\n", len);
        }
        usleep(10000);
    }

    vRdlcDestroy(proto);
    close(fd);
    return 0;
}
