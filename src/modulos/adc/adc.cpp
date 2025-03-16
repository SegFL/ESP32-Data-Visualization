

#include "adc.h"
#include <modulos/ina219/ina219.h>
#include <modulos/queueCom/queueCom.h>

void adcInit() {
    // Inicializar el buffer


    // Configuración de pines
    pinMode(36, INPUT);
}




void  leerADC(){


  ADCData temp;
  int i =0;
  while(i<1){//Cantidad de sensores
    
    temp = {A0, 5.0, 10.0, 2.5, 12.5, millis()};
    if (sendSensorDataToUserInterface(temp)) {
      //Serial.println("Dato enviado: "+String(temp.timestamp));
    } else {
      Serial.println("No se pudo enviar el dato");
    }
    /*
    if(getData(temp,i)==true){
        temp.pin=i; 
        temp.timestamp=micros();
        
    }
    */
    i++;
  }

  
}






