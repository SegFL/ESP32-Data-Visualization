
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "ADCData.h"
#include "../../modulos/time/time.h"
#include "../../config.h"


/*
Las comunicaciones i2c con el sensor se cargan automaticamente(SDA=D21-SCL=D22) pero manualmente las cambio a 
SDA=D32
SCL=D33

*/

void ina219Init();

//Recive como parametro un puntero a un ADCData y el numero de sensor que lee
//Modifica elcontenido de data con losparametros leidos del sensor
bool getData(ADCData& data,int sensor); 

