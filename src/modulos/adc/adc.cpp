
#include <Arduino.h>
#include "adc.h"
#include <modulos/buffer/buffer.h>
#include "ADCData.h"


static Buffer* adcBuffer=NULL;

void adcInit(){
  pinMode(36, INPUT);
}


bool ADCEmpty(){
  return adcBuffer->BufferEmpty();
}

void  leerADC(){
  static Buffer adcBuffer(4);

  ADCData temp;
  int i =0;
  while(i<1){
    temp.value=analogRead(36);
    temp.pin=36;
    temp.timestamp=micros();
    //adcBuffer.addData(temp);
    i++;
  }
  writeSerialCom(String(temp.pin)+","+String(temp.value)+"\n\r");

  //adcBuffer.printData();
  
}

void getADCData(ADCData* dataPtr){
  adcBuffer->copyFirstData(dataPtr);
}




