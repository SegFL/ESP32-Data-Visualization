#ifndef PWM_H
#define PWM_H


#include <esp32-hal-ledc.h>
#include <esp32-hal.h>
#include <HardwareSerial.h>
#include <modulos/serialCom/serialCom.h>


void CargaElectronicaInit();
void CargaElectronicaUpdate();
int PWMSetDC(int dc);
bool PWMSetFrequency(int frecuencies);
bool PWMSetMaxDC(int dc);
#endif
