
#include "ADCData.h"
#include <Arduino.h>
#include <modulos/buffer/buffer.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/ina219/ina219.h>
#include "../queueCom/queueCom.h"


void adcInit() ;
void  leerADC();

bool ADCEmpty();
