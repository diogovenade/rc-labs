// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "statemachine.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 5

#define FLAG 0x7E
#define A_COMTX 0X03 // command sent by transmitter
#define A_REPRX 0x03 // reply sent by receiver
#define A_COMRX 0x01 // command sent by receiver
#define A_REPTX 0x01 // reply sent by transmitter
#define C_SET 0X03
#define C_UA 0x07
#define C_I0 0x00
#define C_I1 0x80

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

int llopenTx(LinkLayer connectionParameters) {
    (void)signal(SIGALRM, alarmHandler);

    StateMachine* statemachine = new_statemachine();

    if (statemachine == NULL) {
        printf("Error allocating state machine\n");
        return -1;
    }

    while (alarmCount < connectionParameters.nRetransmissions) {
        unsigned char buf[BUF_SIZE] = {FLAG, A_COMTX, C_SET, A_COMTX ^ C_SET, FLAG};

        if (writeBytesSerialPort(buf, BUF_SIZE) < 5) {
            printf("Error while writing\n");
            free(statemachine);
            return -1;
        }

        printf("Sent SET frame\n");

        if (alarmEnabled == FALSE) {
            alarm(connectionParameters.timeout);
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

int llopenRx(LinkLayer connectionParameters) {
    unsigned char buf[BUF_SIZE] = {0};
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

    if (writeBytesSerialPort(buf, BUF_SIZE) < BUF_SIZE) {
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

    if (connectionParameters.role == LlTx)
        return llopenTx(connectionParameters);
    
    return llopenRx(connectionParameters);
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

int llwrite(const unsigned char *buf, int bufSize)
{
    static int frameNumber = 0;

    unsigned char frame[bufSize + 6]; // information frame
    frame[0] = FLAG;
    frame[1] = A_COMTX;
    frame[2] = (frameNumber == 0) ? C_I0 : C_I1;
    frame[3] = frame[1] ^ frame[2];
    for (int i = 4; i < bufSize + 4; i++) {
        frame[i] = buf[i - 4];
    }
    frame[4 + bufSize] = bcc2(buf, bufSize);
    frame[5 + bufSize] = FLAG;

    frameNumber = (frameNumber + 1) % 2;
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
