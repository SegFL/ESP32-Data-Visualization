

#include "adc.h"
#include <modulos/ina219/ina219.h>

void adcInit(Buffer*& adcBuffer) {
    // Inicializar el buffer
    adcBuffer = new Buffer();

    // Configuración de pines
    pinMode(36, INPUT);
}




void  leerADC(Buffer*& adcBuffer){


  ADCData temp;
  int i =0;
  while(i<1){
    
    if(getData(temp,i)==false){
        temp.pin=i; 
        temp.timestamp=micros();
        adcBuffer->addData(temp);
    }
    i++;
  }

  
}






