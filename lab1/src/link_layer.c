// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "statemachine.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

static int frameNumber = 0;

int nRetransmissions;
int timeout;

int alarmEnabled = FALSE;
int alarmCount = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

int llopenTx() {
    (void)signal(SIGALRM, alarmHandler);

    StateMachine* statemachine = new_statemachine();

    if (statemachine == NULL) {
        printf("Error allocating state machine\n");
        return -1;
    }

    while (alarmCount < nRetransmissions) {
        unsigned char buf[5] = {FLAG, A_COMTX, C_SET, A_COMTX ^ C_SET, FLAG};

        if (writeBytesSerialPort(buf, 5) < 5) {
            printf("Error while writing\n");
            free(statemachine);
            return -1;
        }

        printf("Sent SET frame\n");

        if (alarmEnabled == FALSE) {
            alarm(timeout);
            alarmEnabled = TRUE;
        }

        int byteindex = 0;

        while (statemachine->state != STOP && alarmEnabled) {
            if (readByteSerialPort(&buf[byteindex]) > 0) {
                change_state(statemachine, buf[byteindex], A_REPRX, C_UA);
                if (statemachine->state == START) {
                    byteindex = 0;
                    continue;
                }
                byteindex++;
            }
        }

        if (statemachine->state == STOP) {
            alarm(0);
            printf("Received UA frame\n");
            free(statemachine);
            return 1;
        }
    }

    printf("Max retransmissions reached. Connection failed.\n");
    free(statemachine);
    return -1;
}

int llopenRx() {
    unsigned char buf[5] = {0};
    int byteindex = 0;
    StateMachine* statemachine = new_statemachine();

    if (statemachine == NULL) {
        printf("Error allocating state machine\n");
        return -1;
    }

    while (statemachine->state != STOP) {
        if (readByteSerialPort(&buf[byteindex]) > 0) {
            change_state(statemachine, buf[byteindex], A_COMTX, C_SET);
            if (statemachine->state == START) {
                byteindex = 0;
                continue;
            }
            byteindex++;
        }
    }

    printf("Received SET frame\n");

    buf[0] = 0x7E;
    buf[1] = 0x03;
    buf[2] = 0x07;
    buf[3] = buf[1] ^ buf[2];
    buf[4] = 0x7E;

    if (writeBytesSerialPort(buf, 5) < 5) {
        printf("Error while writing UA frame\n");
        free(statemachine);
        return -1;
    }

    printf("Sent UA frame\n");
    free(statemachine);
    return 1;
}

int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    nRetransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    if (connectionParameters.role == LlTx)
        return llopenTx();
    
    return llopenRx();
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
unsigned char bcc2(const unsigned char *buf, int bufSize) {
    unsigned char bcc2 = buf[0];
    for (int i = 1; i < bufSize; i++) {
        bcc2 ^= buf[i];
    }
    return bcc2;
}

int stuffing(unsigned char *frame, int bufSize) {
    int length = 4; // initial length, before data
    int frameLength = bufSize + 6; // frame length before stuffing

    unsigned char copyFrame[bufSize + 6];
    memcpy(copyFrame, frame, sizeof(copyFrame));

    for (int i = 4; i < frameLength; i++) {
        if (copyFrame[i] == FLAG && i != (frameLength - 1)) {
            frame = realloc(frame, length + 1);
            frame[length++] = 0x7D;
            frame[length++] = 0x5E;
        } else if (copyFrame[i] == ESC) {
            frame = realloc(frame, length + 1);
            frame[length++] = 0x7D;
            frame[length++] = 0x5D;
        } else {
            frame[length++] = copyFrame[i];
        }
    }

    return length;
}

int llwrite(const unsigned char *buf, int bufSize)
{
    (void)signal(SIGALRM, alarmHandler);

    unsigned char *frame = malloc(bufSize + 6); // information frame
    frame[0] = FLAG;
    frame[1] = A_COMTX;
    frame[2] = (frameNumber == 0) ? C_I0 : C_I1;
    frame[3] = frame[1] ^ frame[2];
    for (int i = 4; i < bufSize + 4; i++) {
        frame[i] = buf[i - 4];
    }
    frame[4 + bufSize] = bcc2(buf, bufSize);
    frame[5 + bufSize] = FLAG;

    printf("Frame before stuffing: ");
    for (int i = 0; i < bufSize + 6; i++) {
        printf("%02X ", frame[i]);
    }
    printf("\n");

    int frameSize = stuffing(frame, bufSize);

    printf("Frame after stuffing: ");
    for (int i = 0; i < frameSize; i++) {
        printf("%02X ", frame[i]);
    }
    printf("\n");

    alarmCount = 0;
    alarmEnabled = FALSE;
    StateMachine* statemachine = new_statemachine();

    if (statemachine == NULL) {
        printf("Error allocating state machine\n");
        return -1;
    }

    while (alarmCount < nRetransmissions) {
        unsigned char response[5] = {0};

        if (writeBytesSerialPort(frame, frameSize) < frameSize) {
            printf("Error while writing information frame\n");
            free(frame);
            free(statemachine);
            return -1;
        }
        printf("Information frame %d sent\n", frameNumber);

        if (!alarmEnabled) {
            alarm(timeout);
            alarmEnabled = TRUE;
        }

        int byteindex = 0;

        while (statemachine->state != STOP && alarmEnabled) {
            if (readByteSerialPort(&response[byteindex]) > 0) {
                change_state(statemachine, response[byteindex], A_REPRX, (frameNumber == 0) ? C_RR1 : C_RR0);
                if (statemachine->state == START) {
                    byteindex = 0;
                    continue;
                }
                byteindex++;
            }
        }

        if (statemachine->state == STOP) {
            if (response[2] == C_REJ0 || response[2] == C_REJ1) {
                alarmEnabled = FALSE;
                printf("Information frame %d rejected, trying again...\n", frameNumber);
            } else {
                alarm(0);
                printf("Reception confirmed\n");
                free(statemachine);
                free(frame);
                frameNumber = (frameNumber + 1) % 2;
                return frameSize;
            }
        }
    }

    printf("Max retransmissions reached. Connection failed.\n");
    free(statemachine);
    free(frame);
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int destuffing(unsigned char *packet, int length) {
    int i = 0, j = 0;
    while (i < length) {
        if (packet[i] == 0x7D) {
            i++;
            if (i < length) {
                if (packet[i] == 0x5E) {
                    packet[j++] = 0x7E;
                } else if (packet[i] == 0x5D) {
                    packet[j++] = 0x7D;
                }
            }
        } else {
            packet[j++] = packet[i];
        }
        i++;
    }
    return j;
}

int llread(unsigned char *packet)
{
    unsigned char byte;
    int byteindex = 0;
    StateMachine* statemachine = new_statemachine();

    if (statemachine == NULL) {
        printf("Error allocating state machine\n");
        return -1;
    }

    unsigned char a_byte = A_COMTX;
    unsigned char c_byte = (frameNumber == 0) ? C_I0 : C_I1;

    volatile int stop = FALSE;

    while (!stop) {
        while (statemachine->state != STOP) {
            if (readByteSerialPort(&byte) > 0) {
                printf("Read byte: %02X\n", byte);
                if (statemachine->state == READ_DATA) {
                    packet[byteindex] = byte;
                    byteindex++;
                }
                change_state(statemachine, byte, a_byte, c_byte);
            }
        }

        if (statemachine->lostReply) {
            frameNumber = (frameNumber + 1) % 2;
        }

        unsigned char reply[5] = {0};

        // First byte is BCC1, last two bytes are BCC2 and FLAG
        if (packet[0] == (a_byte ^ c_byte)) {
            printf("BCC1 validation passed\n");

            unsigned char bcc2Value = packet[byteindex - 2];

            // Remove BCC1, BCC2 and FLAG from packet
            int dataSize = byteindex - 3;
            memmove(packet, packet + 1, dataSize);

            int destuffedSize = destuffing(packet, dataSize);

            if (bcc2Value == bcc2(packet, destuffedSize)) {
                printf("BCC2 validation passed\n");

                reply[0] = FLAG;
                reply[1] = A_REPRX;
                reply[2] = (frameNumber == 0) ? C_RR1 : C_RR0;
                reply[3] = reply[1] ^ reply[2];
                reply[4] = FLAG;

                if (writeBytesSerialPort(reply, 5) < 5) {
                    printf("Error while writing reply to information frame %d\n", frameNumber);
                    free(statemachine);
                    return -1;
                }

                printf("Replied, information frame %d accepted\n", frameNumber);
                printf("Content of the packet (after destuffing):\n");
                for (int i = 0; i < destuffedSize; i++) {
                    printf("%02X ", packet[i]);
                }
                printf("\n\n");
                stop = TRUE;
                free(statemachine);
                frameNumber = (frameNumber + 1) % 2;
                return destuffedSize;

            } else {
                printf("BCC2 validation failed\n");
            }
        } else {
            printf("BCC1 validation failed\n");
        }

        reply[0] = FLAG;
        reply[1] = A_REPRX;
        reply[2] = (frameNumber == 0) ? C_REJ0 : C_REJ1;
        reply[3] = reply[1] ^ reply[2];
        reply[4] = FLAG;

        if (writeBytesSerialPort(reply, 5) < 5) {
            printf("Error while writing reply to information frame %d\n", frameNumber);
            free(statemachine);
            return -1;
        }

        printf("Replied, information frame %d rejected\n", frameNumber);
        statemachine->state = START;
        statemachine->lostReply = FALSE;
    }

    return -1;
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
