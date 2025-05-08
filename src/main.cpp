#include "modulos/adc/adc.h"
#include "modulos/serialCom/serialCom.h"
#include <Arduino.h>
#include "modulos/carga_electronica/carga_electronica.h"
#include "modulos/userInterface/userInterface.h"
#include "modulos/queueCom/queueCom.h"
#include "modulos/ina219/ina219.h"
#include <nvs_flash.h>


#define QUEUE_LENGTH 100       // Máximo número de elementos en la queue
#define ITEM_SIZE sizeof(char) // Tamaño de cada elemento (en este caso, 1 byte para un char)






QueueHandle_t xQueueComSerial;         // Handle para la queue





// Definimos los manejadores de las tareas
TaskHandle_t Task1Handle = NULL;
TaskHandle_t Task2Handle = NULL;

// Función de la tarea 1 RECIVE DATOS
void Task1(void *pvParameters) {//Tarea encargada de administrar la interfaz de usuario

  while (true) {
    userInterfaceUpdate();
    

    
   vTaskDelay(pdMS_TO_TICKS(200)); // Espera 0.5 segundos

  }
}

// Función de la tarea 2
void Task2(void *pvParameters) {//Tarea encargada de leer datos del ADC
  while (true) {
    String buffer="";

    leerADC();
    CargaElectronicaUpdate();



    
    vTaskDelay(pdMS_TO_TICKS(100)); // Espera 0.5 segundos
  }
}


void setup() {
  userInterfaceInit();
  queueInit();
  ina219Init();
  adcInit(); 
  CargaElectronicaInit();
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
        printf("Error: No se pudo crear la queue.\n");
        while (1); // Detener el sistema o manejarlo según sea necesario
    }


    // Crear la tarea 1
  xTaskCreate(
    Task1,          // Función que implementa la tarea
    "Task1",        // Nombre de la tarea
    2000,           // Tamaño del stack en palabras
    NULL,           // Parámetro que se pasa a la tarea
    1,              // Prioridad de la tarea
    &Task1Handle    // Manejador de la tarea
  );

  // Crear la tarea 2
  xTaskCreate(
    Task2,          // Función que implementa la tarea
    "Task2",        // Nombre de la tarea
    2000,           // Tamaño del stack en palabras
    NULL,           // Parámetro que se pasa a la tarea
    1,              // Prioridad de la tarea
    &Task2Handle    // Manejador de la tarea
  );


  //vTaskStartScheduler();




}



void loop() {


}









