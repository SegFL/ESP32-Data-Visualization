

#include "serialCom.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>


#define SERIAL_QUEUE_SIZE 64      // mensajes en cola
#define SERIAL_MSG_MAX_LEN 256    // largo máximo de cada mensaje


#define DATA 0 //Define el tipo de mensaje como dato
#define COMMAND 1 //Define el tipo de mensaje como comando
#define APP_MODE 2 //Define el tipo de mensaje como modo APP
bool MODE_SEND_DATA=true;
void writeSerialComWithChecksum(const String &payload);
uint8_t calculateChecksum(const String &data) ;
static void taskSerialWriter(void *pvParameters);





extern QueueHandle_t serialQueue= nullptr;




void serialComInit() {
    Serial.begin(115200);
    serialQueue = xQueueCreate(SERIAL_QUEUE_SIZE, SERIAL_MSG_MAX_LEN);
    xTaskCreatePinnedToCore(taskSerialWriter, "TaskSerial", 3*2048, NULL, 1, NULL, 0);

}


// Es la UNICA funcion de todo el sistema que toca Serial.print()
static void taskSerialWriter(void *pvParameters) {
    char msg[SERIAL_MSG_MAX_LEN];
    for (;;) {
        // bloquea sin consumir CPU hasta que llegue un mensaje
        if (xQueueReceive(serialQueue, msg, portMAX_DELAY) == pdTRUE) {
            Serial.print(msg);
        }
    }
}


static void enqueueMsg(const String &data) {
    if (serialQueue == nullptr) return;
    char buf[SERIAL_MSG_MAX_LEN];
    data.substring(0, SERIAL_MSG_MAX_LEN - 1).toCharArray(buf, SERIAL_MSG_MAX_LEN);
    // timeout 0: si la cola esta llena, descarta en vez de bloquear
    xQueueSend(serialQueue, buf, 0);

    
}

char readSerialChar() {
    if (Serial.available() > 0) {
        char receivedChar = Serial.read();
        if (receivedChar == '\r') {
            return '\0';  // ignorar CR
        }
        return receivedChar;
    }
    return '\0';
}




void clearScreen() {
    writeSerialCom("\033[2J\033[H");  // Borra pantalla ANSI
}

void moveCursor(int row, int col) {
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "\033[%d;%dH", row, col);
    writeSerialCom(buffer);
}

int serialComAvailable() {
    return Serial.available();
}

void changeMode(bool mode){
    MODE_SEND_DATA=mode;

}


bool sendDataStatus(){
    return MODE_SEND_DATA;
}


// ── API publica: identica a antes, ninguna toca Serial directamente ──
void writeSerialCom(String data)         { enqueueMsg(data); }
void writeSerialCom(int data)            { enqueueMsg(String(data)); }
void writeSerialCom(float data)          { enqueueMsg(String(data)); }
void writeSerialCom(double data)         { enqueueMsg(String(data)); }
void writeSerialCom(unsigned long data)  { enqueueMsg(String(data)); }

void writeSerialComln(String data)       { enqueueMsg(data + "\r\n"); }




// ===================== CHECKSUM SIMPLE XOR =====================
uint8_t calculateChecksum(const String &data) {
    uint8_t sum = 0;
    for (size_t i = 0; i < data.length(); i++) {
        sum ^= (uint8_t)data[i];
    }
    return sum;
}

void writeSerialComWithChecksum(const String &payload) {
    uint8_t checksum = calculateChecksum(payload);
    char buffer[8];
    sprintf(buffer, "*%02X\r\n", checksum);
    enqueueMsg(payload + String(buffer));  // ← único cambio
}

// Estas no se tocan, siguen igual que las tenías
void writeSerialComlnDATA(String data) {

    writeSerialComWithChecksum(String(DATA) + "," + data);


}

void writeSerialComlnCOMMAND(String data) {
    writeSerialComWithChecksum(String(COMMAND) + "," + data);
}

void writeSerialComlnAPP(String data) {
    writeSerialComWithChecksum(String(APP_MODE) + "," + data);
}




// En serialCom.cpp:
void writeSerialComlnDATA(const char* data) {
    char payload[SERIAL_MSG_MAX_LEN];
    uint8_t checksum = 0;
    
    // Construir "0,<data>" y calcular checksum en un solo paso
    int len = snprintf(payload, sizeof(payload), "%d,%s", DATA, data);
    for (int i = 0; i < len; i++) checksum ^= (uint8_t)payload[i];
    
    char final[SERIAL_MSG_MAX_LEN + 8];
    snprintf(final, sizeof(final), "%s*%02X\r\n", payload, checksum);
    
    if (serialQueue != nullptr)
        xQueueSend(serialQueue, final, 0);

    led_write_state(LED_STATE_SENDING_DATA, true);  


}