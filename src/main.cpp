
#include "modulos/adc/adc.h"
#include "modulos/serialCom/serialCom.h"
#include <Arduino.h>
#include "modulos/influxdb/influxdb.h"
#include "modulos/buffer/buffer.h"
#include "modulos/PWM/PWM.h"





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
  String buffer="";
  char receivedChar='\0';
  while (true) {
            // Simula leer datos de la interfaz serie
        
  Serial.println("Task 1 is running");

  while (Serial.available() > 0) { // Leo todos los datos de la terminal serie
  receivedChar = readSerialChar();

  // Procesar el carácter leído
  if (receivedChar != '\n' && receivedChar != '\r') { // Si no es nueva línea ni retorno de carro
    buffer += receivedChar;
  } else if (receivedChar == '\n') { // Si es nueva línea, enviar el mensaje
    if (!buffer.isEmpty()) { // Verificar si el buffer no está vacío
      //Aca deberia ir un switch un estado por cada modulo, donde se asignaria QueueTemp=QueueModuloCorrespondiente
      if (xQueueSend(xQueueComSerial, &buffer, portMAX_DELAY) != pdPASS) {
        Serial.println("Error: No se pudo enviar a la cola.");
      } else {
        Serial.println("Envie:");
        Serial.println(buffer);
      }
      //Aca deberia enviar el buffer al Queue correspondiente
      buffer = ""; // Limpiar el buffer
    }
  }
}


      Serial.println("Task 1 is Stopping");

        // Simular un retardo
    vTaskDelay(pdMS_TO_TICKS(1000)); // Espera 1 segundo
  }
}

// Función de la tarea 2
void Task2(void *pvParameters) {
  while (true) {
    String buffer="";
  
    Serial.println("Task 2 is running");
    if (xQueueReceive(xQueueComSerial, &buffer, portMAX_DELAY) == pdPASS)
        {
            // Procesar el dato recibido
                Serial.print(buffer);
                buffer="";
            // Implementar lógica de conexión o configuración Wi-Fi
        }
    Serial.println("Task 2 is stopping");
    vTaskDelay(pdMS_TO_TICKS(500)); // Espera 0.5 segundos
  }
}


void setup() {

  serialComInit();
  adcInit(adcBuffer); 
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









