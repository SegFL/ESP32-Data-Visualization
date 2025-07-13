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


    if (xQueueSend(xQueueAdcUserInterface, &data, pdMS_TO_TICKS(100)) == pdTRUE) {
        return true;
    }
    return false;
}


// Recibe datos del sensor desde la cola.
//Lee todos los datos pero solo se queda con el ultimo. Estoprovoca que los datos mostrados
//en la interfaz de usuario no sean exactamente los mismos que los que lee el sensor

bool receiveSensorDataToUserInterface(ADCData data[]) {//Leo todos los datos de la cola pero solo me quedo con el ultimo
    if (xQueueAdcUserInterface == NULL) {
        return false;
    }

    // Vaciar la cola antes de leer el último dato disponible
    ADCData tempData;
    while (xQueueReceive(xQueueAdcUserInterface, &tempData, 0) == pdTRUE) {
        switch (tempData.pin){
        case 0:
            data[0] = tempData;
            break;
            case 1:
            data[1] = tempData;
            break;
            case 2:
            data[2] = tempData;
            break;
            case 3:
            data[3] = tempData;
            break;                               
        default:
            break;
        }
    }

    return true; // Se obtuvo al menos un dato
}

