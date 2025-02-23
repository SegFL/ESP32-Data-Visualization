
#include "ADCData.h"
#include <Arduino.h>
#include <modulos/buffer/buffer.h>
#include <modulos/serialCom/serialCom.h>

void adcInit(Buffer*& adcBuffer) ;
void  leerADC(Buffer*& adcBuffer);

bool ADCEmpty();
