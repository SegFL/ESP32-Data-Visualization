
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "ADCData.h"

void ina219Init();

//Recive como parametro un puntero a un ADCData y el numero de sensor que lee
//Modifica elcontenido de data con losparametros leidos del sensor
bool getData(ADCData& data,int sensor); 