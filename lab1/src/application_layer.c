// Application layer protocol implementation

#include "../include/application_layer.h"
#include "../include/link_layer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PAYLOAD_SIZE 25

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

unsigned char* newDataPacket(int sequenceNumber, unsigned char *data, int dataSize, int *length) {
    int l1 = dataSize & 0xFF; // lower byte
    int l2 = (dataSize >> 8) & 0xFF; // upper byte
    *length = dataSize + 4;

    unsigned char* dataPacket = (unsigned char*) malloc(*length);
    int offset = 0;
    dataPacket[offset++] = 2;
    dataPacket[offset++] = sequenceNumber;
    dataPacket[offset++] = l2;
    dataPacket[offset++] = l1;
    memcpy(dataPacket + offset, data, dataSize);

    return dataPacket;
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

    // Sends START control packet
    int lengthControlPacket;
    unsigned char* controlPacket = newControlPacket(1, fileSize, filename, &lengthControlPacket);
    if (llwrite(controlPacket, lengthControlPacket) == -1) {
        printf("Error sending start control packet\n");
        return -1;
    }

    unsigned char *fileContents = (unsigned char *)malloc(fileSize);
    fread(fileContents, 1, fileSize, file);

    long bytes = fileSize;
    int sequenceNumber = 0;

    // Sending data packets
    while (bytes > 0) {
        int size;
        if (bytes > PAYLOAD_SIZE)
            size = PAYLOAD_SIZE;
        else
            size = bytes;
        
        unsigned char* data = (unsigned char*) malloc(size);
        memcpy(data, fileContents, size);

        int packetLength;
        unsigned char* dataPacket = newDataPacket(sequenceNumber, data, size, &packetLength);

        if (llwrite(dataPacket, packetLength) == -1) {
            printf("Error sending data packet\n");
            return -1;
        }

        bytes = bytes - size;
        fileContents = fileContents + size;
        sequenceNumber = (sequenceNumber + 1) % 100;
        free(data);
    }

    // Sends END control packet
    controlPacket = newControlPacket(3, fileSize, filename, &lengthControlPacket);
    if (llwrite(controlPacket, lengthControlPacket) == -1) {
        printf("Error sending end control packet\n");
        return -1;
    }

    fclose(file);

    return 1;
}

unsigned char* readControlPacket(unsigned char* controlPacket, long* fileSize) {
    unsigned char nBytesFileSize = controlPacket[2];
    memcpy(fileSize, controlPacket + 3, nBytesFileSize);

    unsigned char nBytesFileName = controlPacket[3 + nBytesFileSize + 1];
    unsigned char *name = (unsigned char*)malloc(nBytesFileName);
    memcpy(name, controlPacket + 3 + nBytesFileSize + 2, nBytesFileName);

    return name;
}

int applicationLayerRx(const char* filename) {
    unsigned char *packet = (unsigned char*)malloc(PAYLOAD_SIZE);
    if (llread(packet) == -1) { // read start control packet
        printf("Error while reading frame\n");
        return -1;
    }

    long fileSize;
    readControlPacket(packet, &fileSize);
    FILE* file = fopen(filename, "wb");

    int stop = FALSE;

    while (!stop) {
        int packetSize;
        if ((packetSize = llread(packet)) == -1) {
            printf("Error while reading frame\n");
            return -1;
        }
        if (packet[0] == 3 && packetSize != 0) {
            stop = TRUE;
        } else if (packetSize != 0) {
            int dataSize = 256 * packet[2] + packet[3];
            fwrite(&packet[4], sizeof(unsigned char), dataSize, file);
        }
    }

    fclose(file);
    return 1;
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
    } else {
        if (applicationLayerRx(filename) == -1) {
            printf("ERROR\n");
            return;
        }
    }

    if (llclose(TRUE) == -1) {
        printf("ERROR in connection termination\n");
        return;
    }

    printf("Connection terminated\n");
    return;
}
