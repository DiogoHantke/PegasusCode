#ifndef PEGASUS_BUZZER_H
#define PEGASUS_BUZZER_H

#include <stdint.h>

void buzzer_init(void);
void buzzer_beep(uint8_t count);
void buzzer_update(void);

#endif
