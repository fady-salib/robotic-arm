#include "Arduino.h"
uint32_t fake_ms = 0;
int analog_values[32] = {}, digital_values[32] = {};
FakeSerial Serial;
bool fake_interrupts_enabled = true;
