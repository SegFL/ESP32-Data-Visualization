

#include "adc.h"
#include <modulos/ina219/ina219.h>
#include <modulos/queueCom/queueCom.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/userInterface/userInterface.h>
#include <modulos/time/time.h>
#include <modulos/carga_electronica/carga_electronica.h>

float lastCurrent_mA = 0.0f; // Variable para almacenar la última corriente medida
#define NUMBER_OF_SENSORS 1 // Número de sensores INA219: Si se cambia tambien se deberia cambiar el valor en ins219.cpp
void adcInit() {
    // Inicializar el buffer


    // Configuración de pines
    pinMode(36, INPUT);

}


void  leerADC(){

  ADCData temp = {0}; // Inicializar todos los campos a 0
  int i = 0;

  while(i < NUMBER_OF_SENSORS){
      if(getData(temp, i) == true){
        // Solo enviar la corriente al actuador
        //sendToActuator(temp.current_mA); 
        
        sendSensorDataToUserInterface(temp);
        if(sendDataStatus()==true){
          writeSerialComlnDATA(String(temp.timestampMillis)+","+String(temp.current_mA)+","+String(temp.busVoltage_V)+","+String(temp.shuntVoltage_mV)+","+String(temp.power_mW)+","+String(temp.pin));
        }
      }
      lastCurrent_mA = temp.current_mA; // Actualizar la última corriente medida
      i++;
  }
}

float getLastCurrentData(){
  return lastCurrent_mA;
}
