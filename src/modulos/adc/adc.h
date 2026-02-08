#ifndef ADC_H
#define ADC_H
#include "ADCData.h"
#include <Arduino.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/ina219/ina219.h>
#include "../queueCom/queueCom.h"
#include "../../config.h"


void adcInit() ;
void  leerADC();
float getLastCurrentData(int index);
bool ADCEmpty();

#endif // ADC_H