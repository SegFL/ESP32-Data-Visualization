

#include "adc.h"

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
    temp.value=analogRead(36);
    temp.pin=36;
    temp.timestamp=micros();
    adcBuffer->addData(temp);
    i++;
  }
  writeSerialCom(String(temp.pin)+","+String(temp.value)+"\n\r");
  

  //adcBuffer.printData();
  
}






