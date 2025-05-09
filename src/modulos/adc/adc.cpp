

#include "adc.h"
#include <modulos/ina219/ina219.h>
#include <modulos/queueCom/queueCom.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/userInterface/userInterface.h>
#include <modulos/time/time.h>
#define NUMBER_OF_SENSORS 1 // Número de sensores INA219: Si se cambia tambien se deberia cambiar el valor en ins219.cpp
void adcInit() {
    // Inicializar el buffer


    // Configuración de pines
    pinMode(36, INPUT);
}




void  leerADC(){


  ADCData temp;
  int i =0;
  while(i<NUMBER_OF_SENSORS){//Cantidad de sensores
      if(getData(temp,i)==true){
      //temp = {A0, 5.0, 10.0, 2.5, 12.5, millis()};
        if(sendDataStatus() ==true){
            writeSerialComln(String(temp.timestamp)+String(',')+String(temp.shuntVoltage_mV)+String(',')+String(temp.busVoltage_V)+String(',')+String(temp.current_mA)+String(',')+String(temp.power_mW)+String(',')+String(temp.pin)+String(',')+getFormattedDateTime());
          }else{
            if (sendSensorDataToUserInterface(temp)) {
              //Serial.println("Dato enviado: "+String(temp2.timestamp));
            } 
        }
      }
      i++;
  }

  
}






