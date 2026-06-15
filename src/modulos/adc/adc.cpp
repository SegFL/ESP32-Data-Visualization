

#include "adc.h"
#include <modulos/ina219/ina219.h>
#include <modulos/queueCom/queueCom.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/userInterface/userInterface.h>
#include <modulos/time/time.h>
#include <modulos/carga_electronica/carga_electronica.h>
#include <config.h>
float lastCurrent_mA[NUMBER_OF_SENSORS] = {0.0f}; // Variable para almacenar la última corriente medida
float lastBusVoltage_V[NUMBER_OF_SENSORS] = {0.0f}; // Variable para almacenar la última tensión medida
float lastPower_mW[NUMBER_OF_SENSORS] = {0.0f}; // Variable para almacenar la última potencia medida
static SemaphoreHandle_t currentMutex = nullptr;



void adcInit() {

    // Configuración de pines
    pinMode(36, INPUT);
    currentMutex = xSemaphoreCreateMutex();

}


void  leerADC(){

  //ADCData temp = {0,0.0f,0.0f,0.0f,0.0f,0,0};


  ADCData temp = {0}; // Inicializar todos los campos a 0
  int i = 0;

  while(i < NUMBER_OF_SENSORS){

      if(getData(temp, i) == true){
        // Solo enviar la corriente al actuador
        //sendToActuator(temp.current_mA); 
        

        if(sendDataStatus()==true){

          //Envio datos por la terminal serie hacia la app

            char buf[96];
            snprintf(buf, sizeof(buf), "%lu,%.3f,%.3f,%.3f,%.3f,%d",
                temp.timestampMillis,
                temp.current_mA,
                temp.busVoltage_V,
                temp.shuntVoltage_mV,
                temp.power_mW,
                temp.pin
            );
            writeSerialComlnDATA(buf);
        }
        sendSensorDataToUserInterface(temp);
        
      }
       // Actualizar la última corriente medida(para el PID)

       if (xSemaphoreTake(currentMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
            lastCurrent_mA[i] = temp.current_mA;
            lastBusVoltage_V[i] = temp.busVoltage_V;
            lastPower_mW[i] = temp.power_mW;
            xSemaphoreGive(currentMutex);
        }

      i++;
  }

}

float getLastCurrentData(int index) {
    if (index < 0 || index >= NUMBER_OF_SENSORS) return 0.0f;
    
    float val = 0.0f;
    if (xSemaphoreTake(currentMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        val = lastCurrent_mA[index];
        xSemaphoreGive(currentMutex);
    }
    return val;
}

float getLastBusVoltage(int index) {
    if (index < 0 || index >= NUMBER_OF_SENSORS) return 0.0f;
    
    float val = 0.0f;
    if (xSemaphoreTake(currentMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        val = lastBusVoltage_V[index];
        xSemaphoreGive(currentMutex);
    }
    return val;
}

float getLastPowerData(int index){
    if (index < 0 || index >= NUMBER_OF_SENSORS) return 0.0f;
    
    float val = 0.0f;
    if (xSemaphoreTake(currentMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        val = lastPower_mW[index];
        xSemaphoreGive(currentMutex);
    }
    return val;
}