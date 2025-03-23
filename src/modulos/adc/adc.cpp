

#include "adc.h"
#include <modulos/ina219/ina219.h>
#include <modulos/queueCom/queueCom.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/userInterface/userInterface.h>
void adcInit() {
    // Inicializar el buffer


    // Configuración de pines
    pinMode(36, INPUT);
}




void  leerADC(){


  ADCData temp,temp2;
  int i =0;
  while(i<1){//Cantidad de sensores
    

    getData(temp,0);
    //temp = {A0, 5.0, 10.0, 2.5, 12.5, millis()};


    if(sendDataStatus() ==true){
      

      writeSerialComln(String(temp.timestamp)+String(',')+String(temp.shuntVoltage_mV)+String(',')+String(temp.busVoltage_V)+String(',')+String(temp.current_mA)+String(',')+String(temp.power_mW));



    }else{
      if (sendSensorDataToUserInterface(temp)) {
        //Serial.println("Dato enviado: "+String(temp2.timestamp));
      } 
    }
    i++;
  }

  
}






