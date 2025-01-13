
#include "modulos/adc/adc.h"
#include "modulos/serialCom/serialCom.h"
#include <Arduino.h>
#include "modulos/influxdb/influxdb.h"
#include "modulos/buffer/buffer.h"










unsigned long previousMillis = 0; // Tiempo del último evento
const long interval = 1000;      // Intervalo de 1 segundo
unsigned long previousMillisSendingData = 0; // Tiempo del último evento
const long intervalSendingData = 10000;      // Intervalo de 1 segundo
unsigned long previousMillisSensoringData=0;
const long intervalSensoringData = 1000;

static Buffer* adcBuffer=NULL;


void setup() {


  adcInit(adcBuffer);
  serialComInit();
  influxDBInit();


}



void loop() {
    unsigned long currentMillis = millis();
    serialComUpdate();


    
    //Manejo las solicitudes y envios de info de WIFi
    if (currentMillis - previousMillisSensoringData >= intervalSensoringData) {
        previousMillisSensoringData = currentMillis;
        if(adcBuffer!=NULL)
          leerADC(adcBuffer);
    }
    if (currentMillis - previousMillisSendingData >= intervalSendingData) {
        previousMillisSendingData = currentMillis;
        influxDBUpdate(adcBuffer);
    }
 

}









