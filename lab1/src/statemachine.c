#include "../include/statemachine.h"
#include <stdio.h>
#include <stdlib.h>

StateMachine* new_statemachine() {
    StateMachine* statemachine = (StateMachine*) malloc(sizeof(StateMachine));
    statemachine->state = START;
    statemachine->retransmission = FALSE;
    return statemachine;
}

void change_state(StateMachine* statemachine, int byte, unsigned char a_byte, unsigned char c_byte) {
    switch (statemachine -> state) {
        case START:
            printf("START\n");
            if (byte == FLAG) {
                statemachine->state = FLAG_RCV;
            }
            break;
        
        case FLAG_RCV:
            printf("FLAG_RCV\n");
            if (byte == a_byte) {
                statemachine->state = A_RCV;
            } else if (byte == FLAG) {
                statemachine->state = FLAG_RCV;
            } else {
                statemachine->state = START;
            }
            break;
        
        case A_RCV:
            printf("A_RCV\n");
            if (byte == FLAG) {
                statemachine->state = FLAG_RCV;
            } else if (byte == c_byte) {
                if (byte == C_I0 || byte == C_I1) {
                    statemachine->state = READ_DATA;
                }
                else {
                    statemachine->state = C_RCV;
                }
            } else if (byte == C_I0 || byte == C_I1) {
                statemachine->state = READ_DATA;
                statemachine->retransmission = TRUE;
            } else if (byte == C_DISC) {
                statemachine->state = DISC_RCV;
            } else {
                statemachine->state = START;
            }
            break;

        case READ_DATA:
            printf("READ_DATA\n");
            if (byte == FLAG) {
                statemachine->state = STOP;
            }
            break;

        case C_RCV:
            printf("C_RCV\n");
            printf("o famoso byte: %02X, ", byte);
            if (byte == FLAG) {
                statemachine->state = FLAG_RCV;
            } else if (byte == (a_byte ^ c_byte)) {
                statemachine->state = BCC_OK;
            } else {
                printf("BCC ERROR\n");
                statemachine->state = START;
            }
            break;

        case DISC_RCV:
            printf("C_DISC\n");
            if (byte == 0x08) {
                statemachine->state = DISC_BCC_OK;
            }
            break;

        case DISC_BCC_OK:
            printf("DISC_BCC_OK\n");
            if (byte == FLAG) {
                statemachine->state = STOP;
                printf("STOP\n");
            }
            break;
        
        case BCC_OK:
            printf("BCC_OK\n");
            if (byte == FLAG) {
                statemachine->state = STOP;
                printf("STOP\n");
            } else {
                statemachine->state = START;
            }
            break;

        default:
            break;
    }
}
