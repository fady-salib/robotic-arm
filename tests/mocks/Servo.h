#ifndef TEST_SERVO_H
#define TEST_SERVO_H
#include "Arduino.h"
struct Servo {
    int angle = -1;
    bool attached = false, attach_protected = false, initial_write_protected = false;
    void attach(int, int, int) { attach_protected = !fake_interrupts_enabled; attached = true; }
    void write(int n) {
        if (angle < 0) initial_write_protected = attached && !fake_interrupts_enabled;
        angle = n;
    }
};
#endif
