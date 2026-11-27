#ifndef PEGASUS_STATUS_MONITOR_H
#define PEGASUS_STATUS_MONITOR_H
#include <stdint.h>
#include "pegasus_state.h"
void status_monitor_init(void);
void status_monitor_step(STATE state, uint8_t button);
#endif
