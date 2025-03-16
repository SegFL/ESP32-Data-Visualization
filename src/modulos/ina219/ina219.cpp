#include "ina219.h"
#include <modulos/serialCom/serialCom.h>
#include <ADCData.h>


#define NUM_SENSORS 1 // Número de sensores INA219

// Crear un vector de punteros para manejar múltiples sensores
Adafruit_INA219* ina219[NUM_SENSORS];

// Direcciones I2C para cada sensor
uint8_t sensorAddresses[NUM_SENSORS] = {0x40};//0x40,0x41, 0x44

void ina219Init(){
  // Iniciar la comunicación serie
    
    writeSerialComln("Inicializando sensores INA219...");

    // Inicializar los sensores en sus respectivas direcciones
    for (int i = 0; i < NUM_SENSORS; i++) {
        ina219[i] = new Adafruit_INA219(sensorAddresses[i]); // Crear instancia con dirección específica
        if (!ina219[i]->begin()) {
            writeSerialCom("Error al inicializar el sensor INA219 en la dirección 0x");
            writeSerialComln(String(sensorAddresses[i]));
            while (1) { delay(10); }
        }
        ina219[i]->setCalibration_16V_400mA();
        writeSerialCom("INA219 en dirección 0x");
        writeSerialCom(String(sensorAddresses[i]));
        writeSerialComln(" inicializado correctamente.");
    }

}


bool getData(ADCData& data, int sensor){ //Numero del sensor a leer

    // Leer el voltaje del bus

    if(sensor<NUM_SENSORS){
        data.busVoltage_V = ina219[sensor]->getBusVoltage_V();
        data.current_mA = ina219[sensor]->getCurrent_mA();
        data.power_mW = ina219[sensor]->getPower_mW();
        data.shuntVoltage_mV = ina219[sensor]->getShuntVoltage_mV();
        data.timestamp = millis();
        data.pin = sensor;
        return true;
    }else{
        return false;
    }

    
}