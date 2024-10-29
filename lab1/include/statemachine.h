#ifndef _STATE_MACHINE_H_
#define _STATE_MACHINE_H_

#define FLAG 0x7E
#define ESC 0x7D
#define A_COMTX 0X03 // command sent by transmitter
#define A_REPRX 0x03 // reply sent by receiver
#define A_COMRX 0x01 // command sent by receiver
#define A_REPTX 0x01 // reply sent by transmitter
#define C_SET 0X03
#define C_UA 0x07
#define C_I0 0x00
#define C_I1 0x80
#define C_RR0 0xAA
#define C_RR1 0xAB
#define C_REJ0 0x54
#define C_REJ1 0x55

#include "link_layer.h"

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    READ_DATA,
    BCC_OK,
    STOP
} State;

typedef struct
{
    State state;
    int retransmission; // TRUE if same information frame was received, despite expecting next one
} StateMachine;

StateMachine* new_statemachine();
void change_state(StateMachine* statemachine, int byte, unsigned char a_byte, unsigned char c_byte);

#endif
