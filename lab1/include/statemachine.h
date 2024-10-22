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

typedef enum
{
    COMMAND,
    REPLY
} DataType;

typedef enum
{
    SET,
    UA
} Frame;

typedef struct
{
    State state;
    LinkLayerRole role; // role of the data source (e.g. if transmitter is reading UA frame, then role is LlRx)
    DataType datatype;
    Frame frame;
} StateMachine;

StateMachine* new_statemachine(LinkLayerRole role, DataType datatype, Frame frame);
void change_statemachine(StateMachine* statemachine, int byte);
int get_a_byte(StateMachine* statemachine);
int get_c_byte(StateMachine* statemachine);
void delete_statemachine(StateMachine* statemachine);

#endif
