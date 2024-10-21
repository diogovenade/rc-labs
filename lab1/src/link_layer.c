// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include <stdio.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 5

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    if (connectionParameters.role == LlTx) {
        unsigned char buf[BUF_SIZE] = {0};
        buf[0] = 0x7E;
        buf[1] = 0x03;
        buf[2] = 0x03;
        buf[3] = buf[1] ^ buf[2];
        buf[4] = 0x7E;

        int bytes = writeBytesSerialPort(buf, BUF_SIZE);

        if (bytes < 5) {
            printf("Error: fewer than 5 bytes written\n");
            return -1;
        }

        printf("\nSent SET frame:\n");
        for (int i = 0; i < BUF_SIZE; i++) {
                printf("var = 0x%02X\n", (unsigned int)(buf[i]));
        }
        printf("%d bytes written\n\n", bytes);

        unsigned char ua[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
        volatile int STOP = FALSE;
        int bytecounter = 0;

        printf("Received UA frame:\n");

        while (STOP == FALSE) {
            int byte = readByteSerialPort(&ua[bytecounter]);
            if (byte == -1) {
                printf("Error: unable to read byte\n");
                return -1;
            } else if (byte == 0) {
                printf("Error: no byte was received\n");
                return -1;
            }

            printf("var = 0x%02X\n", (unsigned int)(ua[bytecounter]));

            bytecounter++;
            if (bytecounter == BUF_SIZE) {
                ua[bytecounter] = '\0';
                STOP = TRUE;
                printf("Read %d bytes\n", bytecounter);
            }
        }

        if (!(ua[2] == 0x07 && ((ua[1] ^ ua[2]) == ua[3]))) {
            printf("Received frame is incorrect\n");
            return -1;
        }

        return 1;
    }

    else if (connectionParameters.role == LlRx) {
        volatile int STOP = FALSE;
        unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
        int bytecounter = 0;

        printf("\nReceived SET frame:\n");

        while (STOP == FALSE) {
            int byte = readByteSerialPort(&buf[bytecounter]);
            if (byte == -1) {
                printf("Error: unable to read byte\n");
                return -1;
            } else if (byte == 0) {
                printf("Error: no byte was received\n");
                return -1;
            }

            printf("var = 0x%02X\n", (unsigned int)(buf[bytecounter]));

            bytecounter++;
            if (bytecounter == BUF_SIZE) {
                buf[bytecounter] = '\0';
                STOP = TRUE;
                printf("Read %d bytes\n\n", bytecounter);
            }
        }

        if ((buf[1] ^ buf[2]) == buf[3]) {
            unsigned char ua[BUF_SIZE] = {0};

            ua[0] = 0x7E;
            ua[1] = 0x03;
            ua[2] = 0x07;
            ua[3] = ua[1] ^ ua[2];
            ua[4] = 0x7E;

            int bytes_ua = writeBytesSerialPort(ua, BUF_SIZE);

            if (bytes_ua < 5) {
                printf("Error: fewer than 5 bytes written\n");
                return -1;
            }
            printf("Sent UA frame:\n");
            for (int i = 0; i < BUF_SIZE; i++) {
                printf("var = 0x%02X\n", (unsigned int)(ua[i]));
            }
            printf("%d bytes written\n", bytes_ua);
        } else {
            printf("Received frame is incorrect\n");
            return -1;
        }

        return 1;
    }

    else {
        return -1;
    }

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
