
#include "modulos/adc/adc.h"
#include "modulos/serialCom/serialCom.h"
#include <Arduino.h>
#include "modulos/influxdb/influxdb.h"
#include "modulos/buffer/buffer.h"
#include "modulos/PWM/PWM.h"
#include "modulos/userInterface/userInterface.h"
#include "modulos/queueCom/queueCom.h"


#define QUEUE_LENGTH 100       // Máximo número de elementos en la queue
#define ITEM_SIZE sizeof(char) // Tamaño de cada elemento (en este caso, 1 byte para un char)






QueueHandle_t xQueueComSerial;         // Handle para la queue




unsigned long previousMillis = 0; // Tiempo del último evento
const long interval = 1000;      // Intervalo de 1 segundo
unsigned long previousMillisSendingData = 0; // Tiempo del último evento
const long intervalSendingData = 10000;      // Intervalo de 1 segundo
unsigned long previousMillisSensoringData=0;
const long intervalSensoringData = 1000;

static Buffer* adcBuffer=NULL;




// Definimos los manejadores de las tareas
TaskHandle_t Task1Handle = NULL;
TaskHandle_t Task2Handle = NULL;

// Función de la tarea 1 RECIVE DATOS
void Task1(void *pvParameters) {

  while (true) {
    userInterfaceUpdate();

    // Simular un retardo
    vTaskDelay(pdMS_TO_TICKS(10)); // Espera 1 segundo
  }
}

// Función de la tarea 2
void Task2(void *pvParameters) {
  while (true) {
    String buffer="";
  
    
    ADCData d = {A0, (float)rand(), 10.0, 2.5, 12.5, millis()};
    sendSensorDataToUserInterface(d);
      
    vTaskDelay(pdMS_TO_TICKS(500)); // Espera 0.5 segundos
  }
}


void setup() {
  userInterfaceInit();
  //adcInit(adcBuffer); 
  //influxDBInit();

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
    1000,           // Tamaño del stack en palabras
    NULL,           // Parámetro que se pasa a la tarea
    1,              // Prioridad de la tarea
    &Task1Handle    // Manejador de la tarea
  );

  // Crear la tarea 2
  xTaskCreate(
    Task2,          // Función que implementa la tarea
    "Task2",        // Nombre de la tarea
    1000,           // Tamaño del stack en palabras
    NULL,           // Parámetro que se pasa a la tarea
    1,              // Prioridad de la tarea
    &Task2Handle    // Manejador de la tarea
  );


  //vTaskStartScheduler();



/*
  serialComInit();
  adcInit(adcBuffer); 
  influxDBInit();

*/
}



void loop() {

/*
    unsigned long currentMillis = millis();
    serialComUpdate();


    
    //Manejo las solicitudes y envios de info de WIFi
    if (currentMillis - previousMillisSensoringData >= intervalSensoringData) {
        previousMillisSensoringData = currentMillis;
        if(adcBuffer!=NULL)
          leerADC(adcBuffer);
    }
    if (currentMillis - previousMillisSendingData >= intervalSendingData) {
        previousMillisSendingData = currentMillis;
        influxDBUpdate(adcBuffer);
    }
 
*/
}









