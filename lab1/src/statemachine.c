#include "../include/statemachine.h"
#include <stdio.h>
#include <stdlib.h>

StateMachine* new_statemachine() {
    StateMachine* statemachine = (StateMachine*) malloc(sizeof(StateMachine));
    statemachine->state = START;
    statemachine->repeatedFrame = FALSE;
    return statemachine;
}

void change_state(StateMachine* statemachine, int byte, unsigned char a_byte, unsigned char c_byte) {
    switch (statemachine -> state) {
        case START:
            if (byte == FLAG) {
                statemachine->state = FLAG_RCV;
            }
            break;
        
        case FLAG_RCV:
            if (byte == a_byte) {
                statemachine->state = A_RCV;
            } else if (byte == FLAG) {
                statemachine->state = FLAG_RCV;
            } else {
                statemachine->state = START;
            }
            break;
        
        case A_RCV:
            if (byte == FLAG) {
                statemachine->state = FLAG_RCV;
            } else if (byte == c_byte) {
                if (byte == C_I0 || byte == C_I1) {
                    statemachine->state = READ_DATA;
                } else if (byte == C_DISC) {
                    statemachine->state = DISC_RCV;
                }
                else {
                    statemachine->state = C_RCV;
                }
            } else if (byte == C_I0 || byte == C_I1) {
                statemachine->state = READ_DATA;
                statemachine->repeatedFrame = TRUE;
            } else {
                statemachine->state = START;
            }
            break;

        case READ_DATA:
            if (byte == FLAG) {
                statemachine->state = STOP;
            }
            break;

        case C_RCV:
            if (byte == FLAG) {
                statemachine->state = FLAG_RCV;
            } else if (byte == (a_byte ^ c_byte)) {
                statemachine->state = BCC_OK;
            } else {
                statemachine->state = START;
            }
            break;

        case DISC_RCV:
            if (byte == 0x08) {
                statemachine->state = DISC_BCC_OK;
            }
            break;

        case DISC_BCC_OK:
            if (byte == FLAG) {
                statemachine->state = STOP;
            }
            break;
        
        case BCC_OK:
            if (byte == FLAG) {
                statemachine->state = STOP;
            } else {
                statemachine->state = START;
            }
            break;

        default:
            break;
    }
}
