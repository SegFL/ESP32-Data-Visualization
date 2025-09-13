#include <Arduino.h>
#include "modulos/adc/adc.h"
#include "modulos/serialCom/serialCom.h"
#include "modulos/carga_electronica/carga_electronica.h"
#include "modulos/userInterface/userInterface.h"
#include "modulos/queueCom/queueCom.h"
#include "modulos/ina219/ina219.h"
#include "modulos/time/time.h"
#include <nvs_flash.h>

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

void Task2(void *pvParameters) {
  for (;;) {
    leerADC();
    CargaElectronicaUpdate();

    int request = 1;
    xQueueSend(timeRequestQueue, &request, pdMS_TO_TICKS(20));

    vTaskDelay(pdMS_TO_TICKS(100)); // Tiempo suficiente para no saturar el CPU
  }
}

void setup() {
  userInterfaceInit();
  writeSerialComln("=== INICIO DEL SISTEMA ===");
  writeSerialComln(String("Memoria inicial: ") + ESP.getFreeHeap());
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  queueInit();
  ina219Init();
  adcInit();
  TimeInit();
  CargaElectronicaInit();



  xQueueComSerial = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);
  timeRequestQueue = xQueueCreate(10, sizeof(int));

  xTaskCreatePinnedToCore(Task1, "Task1", 4096, NULL, 1, &Task1Handle, 0);
  xTaskCreatePinnedToCore(Task2, "Task2", 4096, NULL, 1, &Task2Handle, 1);

  writeSerialComln("Tareas inicializadas");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
