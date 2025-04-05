#ifndef PWM_H
#define PWM_H


#include <esp32-hal-ledc.h>
#include <esp32-hal.h>
#include <HardwareSerial.h>
#include <modulos/serialCom/serialCom.h>


void PWMInit();
void PWMUpdate();
bool PWMSetDC(int dc);

#endif
