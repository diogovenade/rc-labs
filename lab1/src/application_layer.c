// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;

    strncpy(connectionParameters.serialPort, serialPort, sizeof(connectionParameters.serialPort) - 1);
    connectionParameters.serialPort[sizeof(connectionParameters.serialPort) - 1] = '\0';

    if (strcmp(role, "tx") == 0) {
        connectionParameters.role = LlTx;
    } else if (strcmp(role, "rx") == 0) {
        connectionParameters.role = LlRx;
    } else {
        printf("Invalid role\n");
        return;
    }

    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    if (llopen(connectionParameters) != 1) {
        printf("ERROR\n");
        return;
    }

    unsigned char buf1[4] = {0x12, 0x7E, 0x7D, 0x7E};
    unsigned char buf2[4] = {0x10, 0x7E, 0x14, 0x7D};

    if (connectionParameters.role == LlTx) {
        if (llwrite(buf1, 4) == -1) {
            printf("ERROR\n");
            return;
        }
        if (llwrite(buf2, 4) == -1) {
            printf("ERROR\n");
            return;
        }
    }

    unsigned char packet[1000];

    if (connectionParameters.role == LlRx) {
        if (llread(packet) == -1) {
            printf("ERROR\n");
            return;
        }

        if (llread(packet) == -1) {
            printf("ERROR\n");
            return;
        }
    }

    if (llclose(1) < 0) {
        printf("ERROR while closing the connection\n");
        return;
    }
    
    printf("Connection closed successfully.\n");
    return;

    // TODO
}
