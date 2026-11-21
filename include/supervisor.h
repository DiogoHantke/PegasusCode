#ifndef PEGASUS_SUPERVISOR_H
#define PEGASUS_SUPERVISOR_H
#include "pegasus_state.h"
void supervisor_init(void);
void supervisor_step(STATE *state);
void supervisor_after(STATE *state);
#endif
