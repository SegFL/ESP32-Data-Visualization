#include <Arduino.h>
#include "modulos/adc/adc.h"
#include "modulos/serialCom/serialCom.h"
#include "modulos/carga_electronica/carga_electronica.h"
#include "modulos/userInterface/userInterface.h"
#include "modulos/queueCom/queueCom.h"
#include "modulos/ina219/ina219.h"
#include "modulos/time/time.h"
#include "modulos/dac/dac.h"

#define QUEUE_LENGTH 100
#define ITEM_SIZE sizeof(char)

QueueHandle_t xQueueComSerial;
QueueHandle_t timeRequestQueue;

TaskHandle_t Task1Handle = NULL;
TaskHandle_t Task2Handle = NULL;

void Task1(void *pvParameters) {
  int received;
  for (;;) {
    if (xQueueReceive(timeRequestQueue, &received, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (received == 1) {
        // Evitamos llamadas pesadas aquí
      }
    }

    while (Serial.available() > 0) {
        userInterfaceUpdate();
    }
    TimeUpdate();
    vTaskDelay(pdMS_TO_TICKS(200)); // Cede CPU al resto de tareas
  }
}

/*
Task2 (Core 1)
├── leerADC()
│   ├── getData(sensor 0)  ← I2C ~1ms
│   │   └── writeSerialComlnDATA()  ← String + malloc x6
│   ├── getData(sensor 1)  ← I2C ~1ms
│   │   └── writeSerialComlnDATA()
│   └── ... x5 sensores
│   └── lastCurrent_mA[i] = temp.current_mA  ← variable global sin protección
│
└── CargaElectronicaUpdate()
    └── getLastCurrentData(i)  ← lee lastCurrent_mA[] sin protección



*/
void TaskSensors(void *pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(100);

    for (;;) {
        leerADC();
        vTaskDelayUntil(&lastWake, period);
    }
}
//Tarea que solo controla el PID
void TaskControl(void *pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(100); // PID puede correr más seguido

    for (;;) {
        CargaElectronicaUpdate();
        vTaskDelayUntil(&lastWake, period);
    }
}



void setup() {
  userInterfaceInit();
  writeSerialComln("=== INICIO DEL SISTEMA ===");
  writeSerialComln(String("Memoria inicial: ") + ESP.getFreeHeap());
  nvsInit();
  vTaskDelay(pdMS_TO_TICKS(100));

  queueInit();
  ina219Init();
  adcInit();
  TimeInit();
  CargaElectronicaInit();
  dacInit();


  xQueueComSerial = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);
  timeRequestQueue = xQueueCreate(10, sizeof(int));

  xTaskCreatePinnedToCore(Task1, "Task1", 2*4096, NULL, 1, &Task1Handle, 0);
  //Task2 tendra la priordad maxima ya que se encarga se leer y escribir entradas/salidas

  xTaskCreatePinnedToCore(TaskSensors, "Sensors", 2*4096, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(TaskControl, "Control", 2*4096, NULL, 5, NULL, 1); 
  writeSerialComln("Tareas inicializadas");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}


