#include "statemachine.h"
#include <stdio.h>
#include <stdlib.h>

StateMachine* new_statemachine(LinkLayerRole role, DataType datatype, Frame frame) {
    StateMachine* statemachine = (StateMachine*) malloc(sizeof(StateMachine));
    statemachine->state = START;
    statemachine->role = role;
    statemachine->datatype = datatype;
    statemachine->frame = frame;

    return statemachine;
}

int get_a_byte(StateMachine* statemachine) {
    if ((statemachine->role == LlTx && statemachine->datatype == COMMAND) || (statemachine->role == LlRx && statemachine->datatype == REPLY)) {
        return 0x03; 
    } else {
        return 0x01;
    }
    return 0;
}

int get_c_byte(StateMachine* statemachine) {
    if (statemachine->frame == SET)
        return 0x03;
    else if (statemachine->frame == UA)
        return 0x07;
    return 0;
}

void change_statemachine(StateMachine* statemachine, int byte) {
    switch (statemachine -> state) {
        case START:
            if (byte == 0x7E) {
                statemachine->state = FLAG_RCV;
            }
            break;
        
        case FLAG_RCV:
            if (byte == get_a_byte(statemachine)) {
                statemachine->state = A_RCV;
            } else {
                statemachine->state = START;
            }
            break;
        
        case A_RCV:
            if (byte == 0x7E) {
                statemachine->state = FLAG_RCV;
            } else if (byte == get_c_byte(statemachine)) {
                statemachine->state = C_RCV;
            } else {
                statemachine->state = START;
            }
            break;

        case C_RCV:
            if (byte == 0x7E) {
                statemachine->state = FLAG_RCV;
            } else if (byte == (get_a_byte(statemachine) ^ get_c_byte(statemachine))) {
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

void delete_statemachine(StateMachine* statemachine) {
    if (statemachine != NULL) {
        free(statemachine);
    }
}
