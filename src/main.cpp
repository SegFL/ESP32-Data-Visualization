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

    userInterfaceUpdate(); // Mantenerla si no bloquea más de unos ms
    TimeUpdate();
    vTaskDelay(pdMS_TO_TICKS(200)); // Cede CPU al resto de tareas
  }
}

//Esta tarea se ejecuta cada 200mSmediante un timer,asi el tiempo de ejecucion no se acumula por lo que
// no provoca delays constantesy un muestreo NO uniforme
void Task2(void *pvParameters)
{
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(200);

    for (;;)
    {
        leerADC();
        CargaElectronicaUpdate();
        //dacUpdate();

        int request = 1;
        xQueueSend(timeRequestQueue, &request, 0); // no bloquear

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

  xTaskCreatePinnedToCore(Task1, "Task1", 4096, NULL, 1, &Task1Handle, 0);
  //Task2 tendra la priordad maxima ya que se encarga se leer y escribir entradas/salidas

  xTaskCreatePinnedToCore(Task2, "Task2", 4096, NULL, configMAX_PRIORITIES - 1, &Task2Handle, 1);

  writeSerialComln("Tareas inicializadas");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}


