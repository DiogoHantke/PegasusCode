#ifndef PEGASUS_STATE_H
#define PEGASUS_STATE_H

typedef enum {

    INIT,
    CALIBRATION,
    WAITING,
    RUNNING,
    FINISH,
    STOP,
    EMERGENCY

} STATE;

#endif
