#ifndef PWM_H
#define PWM_H


#include <esp32-hal-ledc.h>
#include <esp32-hal.h>
#include <HardwareSerial.h>
#include <modulos/serialCom/serialCom.h>

//selecciona si se usa un PID o un lazo abierto de control
typedef enum {
    PID,
    NONE
} modoFuncionamiento_t;
//selecciona de donde viene la referencia
typedef enum {
    interface,//Interfaz de usuario(manual)
    curve//Simulador de curvas
    //mixto: Podria agregar un mix de los 2 modos anteriores: Le hago caso a la curva
    //Pero dejo la posibilidad de setear el valor actual al usuario
} referenceMode_t;

void CargaElectronicaInit();
void CargaElectronicaUpdate();
int PWMSetDC(int dc);
bool PWMSetFrequency(int frecuencies);
bool PWMSetMaxDC(int dc);
void sendToActuator(int current_mA);
#endif
