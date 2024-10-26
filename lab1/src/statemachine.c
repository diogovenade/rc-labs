#include "statemachine.h"
#include <stdio.h>
#include <stdlib.h>

StateMachine* new_statemachine() {
    StateMachine* statemachine = (StateMachine*) malloc(sizeof(StateMachine));
    statemachine->state = START;

    return statemachine;
}

void change_state(StateMachine* statemachine, int byte, unsigned char a_byte, unsigned char c_byte) {
    switch (statemachine -> state) {
        case START:
            if (byte == 0x7E) {
                statemachine->state = FLAG_RCV;
            }
            break;
        
        case FLAG_RCV:
            if (byte == a_byte) {
                statemachine->state = A_RCV;
            } else {
                statemachine->state = START;
            }
            break;
        
        case A_RCV:
            if (byte == 0x7E) {
                statemachine->state = FLAG_RCV;
            } else if (byte == c_byte) {
                statemachine->state = C_RCV;
            } else {
                statemachine->state = START;
            }
            break;

        case C_RCV:
            if (byte == 0x7E) {
                statemachine->state = FLAG_RCV;
            } else if (byte == (a_byte ^ c_byte)) {
                statemachine->state = BCC_OK;
            } else {
                statemachine->state = START;
            }
            break;
        
        case BCC_OK:
            if (byte == 0x7E) {
                statemachine->state = STOP;
            } else {
                statemachine->state = START;
            }
            break;

        default:
            break;
    }
}
