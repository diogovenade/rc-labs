// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned char* newControlPacket(int controlField, long fileSize, const char *filename, int *length) {
    size_t lenFileSize = sizeof(fileSize);
    size_t lenFileName = strlen(filename);
    *length = lenFileSize + lenFileName + 5;

    unsigned char* controlPacket = (unsigned char*) malloc(*length);
    int offset = 0;
    controlPacket[offset++] = controlField;
    controlPacket[offset++] = 0; // 0 for file size
    controlPacket[offset++] = lenFileSize;
    memcpy(controlPacket + offset, &fileSize, lenFileSize);
    offset += lenFileSize;
    controlPacket[offset++] = 1;
    controlPacket[offset++] = lenFileName;
    memcpy(controlPacket + offset, filename, lenFileName);

    return controlPacket;
}

int applicationLayerTx(const char *filename) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file\n");
        return -1;
    }

    fseek(file, 0, SEEK_END); // move pointer to end of file
    long fileSize = ftell(file);
    rewind(file); // reset pointer to beginning of file

    int lengthControlPacket;
    unsigned char* controlPacket = newControlPacket(1, fileSize, filename, &lengthControlPacket);

    if (llwrite(controlPacket, lengthControlPacket) == -1) {
        printf("Error sending start control packet\n");
        return -1;
    }

    return -1;

}

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
        printf("ERROR in connection establishment\n");
        return;
    }

    if (connectionParameters.role == LlTx) {
        if (applicationLayerTx(filename) == -1) {
            printf("ERROR\n");
            return;
        }
    }

    /*unsigned char buf1[4] = {0x12, 0x7E, 0x7D, 0x7E};
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

    if (llclose(1) != 1) {
        printf("ERROR while closing the connection\n");
        return;
    }
    
    printf("Connection closed successfully.\n");
    return;*/
}
