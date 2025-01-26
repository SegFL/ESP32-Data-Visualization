#include "ina219.h"
#include <modulos/serialCom/serialCom.h>



Adafruit_INA219 ina219;

void ina219Init(){
    // Iniciar la comunicación serie para depuración



    writeSerialComln("Inicializando INA219...");

    //Crea una conexion I2C con el sensor
    if (!ina219.begin()) {
        writeSerialComln("No se pudo encontrar el sensor INA219. Verifica las conexiones.");
        while (1) { delay(10); } // Detener ejecución si no se encuentra el sensor
    }

    // Configuración opcional del rango del sensor
    ina219.setCalibration_16V_400mA(); // Configuración para 32V y 2A
    writeSerialComln("INA219 inicializado correctamente.");


}


void ina219Update(){

    // Leer el voltaje del bus
    float busVoltage = ina219.getBusVoltage_V();

    // Leer el voltaje del shunt
    float shuntVoltage = ina219.getShuntVoltage_mV();

    // Leer la corriente en mA
    float current_mA = ina219.getCurrent_mA();

    // Leer la potencia en mW
    float power_mW = ina219.getPower_mW();


    writeSerialCom("Voltaje Bus: "); writeSerialCom(String(busVoltage)); writeSerialComln(" V");
    writeSerialCom("Voltaje Shunt: "); writeSerialCom(String(shuntVoltage)); writeSerialComln(" mV");
    writeSerialCom("Corriente: "); writeSerialCom(String(current_mA)); writeSerialComln(" mA");
    writeSerialCom("Potencia: "); writeSerialCom(String(power_mW)); writeSerialComln(" mW");
    writeSerialComln("-----------------------------------");


    
}