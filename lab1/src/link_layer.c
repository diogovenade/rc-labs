// Link layer protocol implementation

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "../include/link_layer.h"
#include "../include/statemachine.h"
#include "../include/serial_port.h"

static int frameNumber = 0;

int nRetransmissions;
int timeout;
LinkLayer gConnectionParameters;

int numRetransmissions = 0;
int numTimeouts = 0;
int numFrames = 0;

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    numTimeouts++;
    printf("Alarm #%d\n", alarmCount);
}

int sendFrameAndWaitForResponse(unsigned char *frame, int frameSize, unsigned char *expectedResponse, int expectedResponseSize, StateMachine *statemachine) {
    while (alarmCount < nRetransmissions) {
        if (writeBytesSerialPort(frame, frameSize) < frameSize) {
            printf("Error while writing frame\n");
            return -1;
        }
        numFrames++;

        printf("Sent frame\n");

        if (!alarmEnabled) {
            alarm(timeout);
            alarmEnabled = TRUE;
        }

        unsigned char response[expectedResponseSize];
        int byteindex = 0;

        while (statemachine->state != STOP && alarmEnabled) {
            if (readByteSerialPort(&response[byteindex]) > 0) {
                change_state(statemachine, response[byteindex], expectedResponse[1], expectedResponse[2]);
                if (statemachine->state == START) {
                    byteindex = 0;
                    continue;
                }
                byteindex++;
            }
        }

        if (statemachine->state == STOP) {
            alarm(0);
            printf("Received expected response\n");
            return 1;
        }

        numRetransmissions++;
    }

    printf("Max retransmissions reached. Connection failed.\n");
    return -1;
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

    unsigned char setFrame[5] = {FLAG, A_COMTX, C_SET, A_COMTX ^ C_SET, FLAG};
    unsigned char uaFrame[5] = {FLAG, A_REPRX, C_UA, A_REPRX ^ C_UA, FLAG};

    int result = sendFrameAndWaitForResponse(setFrame, 5, uaFrame, 5, statemachine);
    free(statemachine);
    return result;
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

    unsigned char uaFrame[5] = {FLAG, A_REPRX, C_UA, A_REPRX ^ C_UA, FLAG};
    if (writeBytesSerialPort(uaFrame, 5) < 5) {
        printf("Error while writing UA frame\n");
        free(statemachine);
        return -1;
    }
    numFrames++;

    printf("Sent UA frame\n");
    free(statemachine);
    return 1;
}

int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0) {
        return -1;
    }

    nRetransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    gConnectionParameters = connectionParameters;

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

int stuffing(unsigned char **frame, int bufSize) {
    int length = 4; // initial length, before data
    int frameLength = bufSize + 6; // frame length before stuffing
    int newSize = frameLength; // Start with original frame size

    unsigned char *copyFrame = (unsigned char*) malloc(newSize);
    if (copyFrame == NULL) {
        printf("Error: Unable to allocate memory for copyFrame\n");
        return -1;
    }
    memcpy(copyFrame, *frame, newSize);

    for (int i = 4; i < frameLength; i++) {
        if (copyFrame[i] == FLAG && i != (frameLength - 1)) {
            newSize += 1;
            *frame = realloc(*frame, newSize);
            if (*frame == NULL) {
                printf("Error: Unable to reallocate memory for stuffed frame\n");
                free(copyFrame);
                return -1;
            }
            (*frame)[length++] = 0x7D;
            (*frame)[length++] = 0x5E;
        } else if (copyFrame[i] == ESC) {
            newSize += 1;
            *frame = realloc(*frame, newSize);
            if (*frame == NULL) {
                printf("Error: Unable to reallocate memory for stuffed frame\n");
                free(copyFrame);
                return -1;
            }
            (*frame)[length++] = 0x7D;
            (*frame)[length++] = 0x5D;
        } else {
            (*frame)[length++] = copyFrame[i];
        }
    }

    free(copyFrame);
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

    int frameSize = stuffing(&frame, bufSize);

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

    unsigned char rrFrame[5] = {FLAG, A_REPRX, (frameNumber == 0) ? C_RR1 : C_RR0, A_REPRX ^ ((frameNumber == 0) ? C_RR1 : C_RR0), FLAG};
    int result = sendFrameAndWaitForResponse(frame, frameSize, rrFrame, 5, statemachine);

    if (result == 1) {
        frameNumber = (frameNumber + 1) % 2;
    }

    free(statemachine);
    free(frame);
    return result;
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
                if (statemachine->state == READ_DATA) {
                    packet[byteindex] = byte;
                    byteindex++;
                }
                change_state(statemachine, byte, a_byte, c_byte);
            }
        }

        unsigned char reply[5] = {0};

        if (statemachine->retransmission) { // if it's a retranmission (e.g. due to lost reply), we can discard duplicate packet
            frameNumber = (frameNumber + 1) % 2;
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
            numFrames++;

            printf("Retransmission of information frame %d, ignoring\n", frameNumber);
            free(statemachine);
            frameNumber = (frameNumber + 1) % 2;
            return 0; // nothing was written to packet
        }

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
                numFrames++;

                printf("Replied, information frame %d accepted\n", frameNumber);
                printf("Content of the packet (after destuffing):\n");
                for (int i = 0; i < destuffedSize; i++) {
                    printf("%02X ", packet[i]);
                }
                printf("\n");
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
        numFrames++;

        printf("Replied, information frame %d rejected\n", frameNumber);
        statemachine->state = START;
        statemachine->retransmission = FALSE;
    }

    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llcloseTx() {
    unsigned char discFrame[5] = {FLAG, A_COMTX, C_DISC, A_COMTX ^ C_DISC, FLAG};
        unsigned char uaFrame[5] = {FLAG, A_REPTX, C_UA, A_REPTX ^ C_UA, FLAG};
        StateMachine* statemachine = new_statemachine();

        if (statemachine == NULL) {
            printf("Error allocating state machine\n");
            return -1;
        }

        if (sendFrameAndWaitForResponse(discFrame, 5, discFrame, 5, statemachine) == -1) {
            free(statemachine);
            return -1;
        }

        if (writeBytesSerialPort(uaFrame, 5) < 5) {
            printf("Error while writing UA frame\n");
            free(statemachine);
            return -1;
        }
        numFrames++;

        printf("Sent UA frame\n");
        free(statemachine);

        return 1;
}

int llcloseRx() {
    unsigned char discFrame[5] = {FLAG, A_REPRX, C_DISC, A_REPRX ^ C_DISC, FLAG};
    StateMachine* statemachine = new_statemachine();

    if (statemachine == NULL) {
        printf("Error allocating state machine\n");
        return -1;
    }

    unsigned char response[5] = {0};
    int byteindex = 0;

    while (statemachine->state != STOP) {
        if (readByteSerialPort(&response[byteindex]) > 0) {
            change_state(statemachine, response[byteindex], A_COMTX, C_DISC);
            if (statemachine->state == START) {
                byteindex = 0;
                continue;
            }
            byteindex++;
        }
    }

    printf("Received DISC frame\n");

    if (writeBytesSerialPort(discFrame, 5) < 5) {
        printf("Error while writing DISC frame\n");
        free(statemachine);
        return -1;
    }
    numFrames++;

    printf("Sent DISC frame\n");

    byteindex = 0;
    statemachine->state = START;

    while (statemachine->state != STOP) {
        if (readByteSerialPort(&response[byteindex]) > 0) {
            change_state(statemachine, response[byteindex], A_REPTX, C_UA);
            if (statemachine->state == START) {
                byteindex = 0;
                continue;
            }
            byteindex++;
        }
    }

    if (statemachine->state == STOP) {
        printf("Received UA frame\n");
        free(statemachine);
    }

    return 1;
}

int llclose(int showStatistics)
{
    (void)signal(SIGALRM, alarmHandler);

    if (gConnectionParameters.role == LlTx) {
        if(llcloseTx() < 0) return -1;
    } else {
        if(llcloseRx() < 0) return -1;
    }

    if (showStatistics) {
        printf("Communication Statistics:\n");
        printf("Number of frames: %d\n", numFrames);
        printf("Number of retransmissions: %d\n", numRetransmissions);
        printf("Number of timeouts: %d\n", numTimeouts);
    }

    return closeSerialPort();
}