
#include "modulos/adc/adc.h"
#include "modulos/serialCom/serialCom.h"
//#include "modulos/server/server.h"
#include <Arduino.h>
#include "modulos/influxdb/influxdb.h"










unsigned long previousMillis = 0; // Tiempo del último evento
const long interval = 1000;      // Intervalo de 1 segundo
unsigned long previousMillisSendingData = 0; // Tiempo del último evento
const long intervalSendingData = 1000;      // Intervalo de 1 segundo

void setup() {


  adcInit();
  serialComInit();
  influxDBInit();


}



void loop() {
    unsigned long currentMillis = millis();
    serialComUpdate();


    
    //Manejo las solicitudes y envios de info de WIFi
    if (currentMillis - previousMillisSendingData >= intervalSendingData) {
        previousMillisSendingData = currentMillis;
        //influxDBUpdate();
        leerADC();

    }
 

}









