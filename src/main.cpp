#include "modulos/adc/adc.h"
#include "modulos/serialCom/serialCom.h"
#include <Arduino.h>
#include "modulos/carga_electronica/carga_electronica.h"
#include "modulos/userInterface/userInterface.h"
#include "modulos/queueCom/queueCom.h"
#include "modulos/ina219/ina219.h"
#include <nvs_flash.h>
#include "modulos/time/time.h"
#include "modulos/simuladorCurvas/simuladorCurvas.h"
#define QUEUE_LENGTH 100       // Máximo número de elementos en la queue
#define ITEM_SIZE sizeof(char) // Tamaño de cada elemento (en este caso, 1 byte para un char)






QueueHandle_t xQueueComSerial;         // Handle para la queue
QueueHandle_t timeRequestQueue;







// Definimos los manejadores de las tareas
TaskHandle_t Task1Handle = NULL;
TaskHandle_t Task2Handle = NULL;

// Función de la tarea 1 RECIVE DATOS
void Task1(void *pvParameters) {//Tarea encargada de administrar la interfaz de usuario
  int received;

  while (true) {
            // Espera hasta 50 ms por una petición
    if (xQueueReceive(timeRequestQueue, &received, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Procesa la petición
        if (received == 1) {
                TimeUpdate(); // Solo Task1 llama a TimeUpdate
        }
    }
    //Serial.printf("Hilo 1 init:Heap libre: %u bytes\n", ESP.getFreeHeap());
    userInterfaceUpdate();
    //Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

    //Serial.printf("Hilo 1 end:Heap libre: %u bytes\n", ESP.getFreeHeap());

   vTaskDelay(pdMS_TO_TICKS(100)); 


  }
}

// Función de la tarea 2
void Task2(void *pvParameters) {//Tarea encargada de leer datos del ADC
  while (true) {
    //Serial.printf("Hilo 2 init:Heap libre: %u bytes\n", ESP.getFreeHeap());



    leerADC();
    //Serial.printf("Hilo 2 medio:Heap libre: %u bytes\n", ESP.getFreeHeap());

    CargaElectronicaUpdate();
    //Serial.printf("Hilo 2 end:Heap libre: %u bytes\n", ESP.getFreeHeap());

    // Ejemplo: pedir actualización de tiempo
    int request = 1; // El valor puede ser un código de acción
    xQueueSend(timeRequestQueue, &request, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(300)); //
 
  }
}



void setup() {
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  userInterfaceInit();

  queueInit();
  ina219Init();
  adcInit(); 
  CargaElectronicaInit();
  TimeInit();
  // Inicializar NVS antes de usarlo(MEMORIA ESTATICA EN LA QUE SE ALMACENAN LA CONFIGURACION DEL SISTEMA)
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }


    // Crear la queue
    xQueueComSerial = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);
    
    // Verificar si la queue se creó correctamente
    if (xQueueComSerial == NULL)
    {
        // Manejar error: No se pudo crear la queue
        writeSerialComln(String("Error: No se pudo crear la queue.\n"));
        while (1); // Detener el sistema o manejarlo según sea necesario
    }
        // Crea la queue con espacio para 10 enteros
    timeRequestQueue = xQueueCreate(10, sizeof(int));
    if (timeRequestQueue == NULL) {
        writeSerialComln(String("Error: No se pudo crear la queue de tiempo") );
        while (1); // Detener el sistema si falla
    }


    // Crear la tarea 1
  xTaskCreate(
    Task1,          // Función que implementa la tarea
    "Task1",        // Nombre de la tarea

    20000,           // Tamaño del stack en palabras
    NULL,           // Parámetro que se pasa a la tarea
    1,              // Prioridad de la tarea
    &Task1Handle    // Manejador de la tarea
  );

  // Crear la tarea 2
  xTaskCreate(
    Task2,          // Función que implementa la tarea
    "Task2",        // Nombre de la tarea


    20000,           // Tamaño del stack en palabras

    NULL,           // Parámetro que se pasa a la tarea
    1,              // Prioridad de la tarea
    &Task2Handle    // Manejador de la tarea
  );

    // Crear la tarea 2



  writeSerialComln(String("------Termine de inicializar las tareas"));

  vTaskStartScheduler();




}



void loop() {


}









