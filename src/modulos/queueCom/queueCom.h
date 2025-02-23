
#include <Arduino.h>
#include <queue>
#include <ADCData.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <Arduino.h>

#include "../serialCom/serialCom.h"

void initQueue();

bool sendSensorDataToUserInterface(ADCData data);//adc->userInterface
bool receiveSensorDataToUserInterface(ADCData &data);//userInterface->adc(No se deberia utilizar)
void queueInit();

