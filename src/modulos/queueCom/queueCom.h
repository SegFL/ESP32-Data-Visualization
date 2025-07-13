
#include <Arduino.h>
#include <queue>
#include <ADCData.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <Arduino.h>

#include "../serialCom/serialCom.h"

void initQueue();

bool sendSensorDataToUserInterface(ADCData data);//adc->userInterface

//Recive un vector de 4 datos de ADCData
bool receiveSensorDataToUserInterface(ADCData data[]);//userInterface->adc(No se deberia utilizar)
void queueInit();


