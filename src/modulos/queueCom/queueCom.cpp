#include "queueCom.h"
#define QUEUE_LENGTH 100       // Máximo número de elementos en la cola
#define ITEM_SIZE sizeof(ADCData) // Tamaño de cada elemento (en este caso, un struct ADCData)

QueueHandle_t xQueueAdcUserInterface;  // Cola para los datos de ADC

// Suponemos que tienes una estructura ADCData


// Función para inicializar la cola
void queueInit(){
    // Crear la cola para los datos del sensor
    xQueueAdcUserInterface = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);
    if (xQueueAdcUserInterface == NULL) {
        writeSerialComln("Error al crear la cola");
    }
}

// Enviar datos del sensor a la cola
bool sendSensorDataToUserInterface(ADCData data){

    return false;
    if (xQueueSend(xQueueAdcUserInterface, &data, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Éxito al enviar los datos a la cola
        return true;
    }
    return false; // Cola llena o error
}

// Recibir datos del sensor desde la cola
bool receiveSensorDataToUserInterface(ADCData &data) {
    // Intentamos recibir datos desde la cola
    if (xQueueAdcUserInterface != NULL) {
        if (xQueueReceive(xQueueAdcUserInterface, &data, 0) == pdTRUE) {
            return true; // Datos recibidos correctamente
        }
    }
        
    return false; // Cola vacía o error al recibir datos
}