#ifndef PEGASUS_SUPERVISOR_H
#define PEGASUS_SUPERVISOR_H
#include "pegasus_state.h"
#include <stdint.h>

/* Parametros configuraveis; sem algoritmo de PID ou corrida neste modulo. */
typedef struct {

    uint16_t kp;
    uint16_t ki;
    uint16_t kd;

} SUPERVISOR_GAINS;

const SUPERVISOR_GAINS *supervisor_get_gains(void);
uint8_t supervisor_button_allowed(void);
uint8_t supervisor_emergency_active(void);
void supervisor_init(void);
void supervisor_step(STATE *state);
void supervisor_after(STATE *state);
#endif
