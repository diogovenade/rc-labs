// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "statemachine.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 5

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

int llopen_tx(LinkLayer connectionParameters) {
    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount < connectionParameters.nRetransmissions) {
        unsigned char buf[BUF_SIZE] = {0};
        buf[0] = 0x7E;
        buf[1] = 0x03;
        buf[2] = 0x03;
        buf[3] = buf[1] ^ buf[2];
        buf[4] = 0x7E;

        int bytes = writeBytesSerialPort(buf, BUF_SIZE);

        if (bytes < 5) {
            printf("Error while writing\n");
            return -1;
        }

        printf("Sent SET frame\n");

        alarm(connectionParameters.timeout);
        alarmEnabled = TRUE;

        unsigned char ua[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
        int bytecounter = 0;
        StateMachine* statemachine = new_statemachine(LlRx, REPLY, UA); // UA frame comes from receiver, so role is LlRx

        while (statemachine->state != STOP && alarmEnabled) {
            int read_byte = readByteSerialPort(&ua[bytecounter]);
            if (read_byte > 0) {
                change_statemachine(statemachine, ua[bytecounter]);
                if (statemachine->state == START) {
                    bytecounter = 0;
                    continue;
                }
                bytecounter++;
            }
        }

        if (statemachine->state == STOP) {
            alarm(0);
            printf("Received UA frame\n");
            return 1;
        }

        printf("Timeout, retransmitting SET frame...\n");
    }

    printf("Max retransmissions reached. Connection failed.\n");
    return -1;
}

int llopen_rx(LinkLayer connectionParameters) {
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    int bytecounter = 0;
    StateMachine* statemachine = new_statemachine(LlTx, COMMAND, SET); // SET frame comes from transmitter, so role is LlTx

    while (statemachine->state != STOP) {
        int read_byte = readByteSerialPort(&buf[bytecounter]);
        if (read_byte > 0) {
            change_statemachine(statemachine, buf[bytecounter]);
            if (statemachine->state == START) {
                bytecounter = 0;
                continue;
            }
            bytecounter++;
        }
    }

    if (statemachine->state == STOP) {
        printf("Received SET frame\n");

        unsigned char ua[BUF_SIZE] = {0};

        ua[0] = 0x7E;
        ua[1] = 0x03;
        ua[2] = 0x07;
        ua[3] = ua[1] ^ ua[2];
        ua[4] = 0x7E;

        int bytes_ua = writeBytesSerialPort(ua, BUF_SIZE);

        if (bytes_ua < BUF_SIZE) {
            printf("Error while writing UA frame\n");
            return -1;
        }

        printf("Sent UA frame\n");
        return 1;
    }

    return -1;
}

int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    if (connectionParameters.role == LlTx)
        return llopen_tx(connectionParameters);
    
    return llopen_rx(connectionParameters);
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
