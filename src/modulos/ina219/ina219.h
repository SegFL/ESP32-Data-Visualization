
#include <Wire.h>
//#include <Adafruit_INA219.h>
#include "ADCData.h"
#include "../../modulos/time/time.h"
#include "../../config.h"


/*
Las comunicaciones i2c con el sensor se cargan automaticamente(SDA=D21-SCL=D22) pero manualmente las cambio a 
SDA=D32
SCL=D33

*/
void ina219Init();

void leerINA219(ADCData dataArr[NUMBER_OF_SENSORS]);
void ads_prepareVoltage(ADCData dataArr[NUMBER_OF_SENSORS]);
void ads_readVoltage(ADCData dataArr[NUMBER_OF_SENSORS], uint32_t timestamp);
void ads_prepareCurrent(ADCData dataArr[NUMBER_OF_SENSORS]);
// PRECONDICIÓN: requiere que ads_readVoltage() se haya llamado antes
// en el mismo ciclo, para que busVoltage_V esté actualizado.

void ads_readCurrent(ADCData dataArr[NUMBER_OF_SENSORS], uint32_t timestamp);
