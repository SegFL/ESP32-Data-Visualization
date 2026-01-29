#ifndef CARGA_ELECTRONICA_H
#define CARGA_ELECTRONICA_H

#include <esp32-hal-ledc.h>
#include <esp32-hal.h>
#include <HardwareSerial.h>
#include <modulos/serialCom/serialCom.h>
#include "../../modulos/pid/pid.h"
#include "../../modulos/adc/adc.h"
#include "../../modulos/nvs/nvs.h"

#define MODO_CURVA "modo_curva" // Clave NVS para modo curva




//selecciona si se usa un PID o un lazo abierto de control
typedef enum {
    PID,
    NONE
} modoFuncionamiento_t;
//selecciona de donde viene la referencia
typedef enum {
    interface_state,//Interfaz de usuario(manual)
    curve_state//Simulador de curvas
    //mixto: Podria agregar un mix de los 2 modos anteriores: Le hago caso a la curva
    //Pero dejo la posibilidad de setear el valor actual al usuario
} referenceMode_t;

typedef enum {
    ON_t,
    OFF_t
} curve_mode_t;

void CargaElectronicaInit();
void CargaElectronicaUpdate();
float PWMSetDC(float dc);
bool PWMSetFrequency(int frecuencies);

bool PWMSetMaxDC(float dc);
void PWMSetCurveMode(curve_mode_t mode);

void printCargaElectronica(); 
void changeControlMode(modoFuncionamiento_t mode);
float PWMGetMaxDC();
bool setMaxCurrent(float current);
modoFuncionamiento_t getModoFuncionamiento();
bool setCurrentReference_mA(float current_mA,int index);
bool getPWMConfig(char index, int *channel, int *freq, int *resolution);
#endif
