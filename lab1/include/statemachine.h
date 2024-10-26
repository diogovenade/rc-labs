#ifndef _STATE_MACHINE_H_
#define _STATE_MACHINE_H_

#include "link_layer.h"

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} State;

typedef struct
{
    State state;
} StateMachine;

StateMachine* new_statemachine();
void change_state(StateMachine* statemachine, int byte, unsigned char a_byte, unsigned char c_byte);

#endif
