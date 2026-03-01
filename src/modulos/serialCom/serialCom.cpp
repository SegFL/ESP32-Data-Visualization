

#include "serialCom.h"


#define DATA 0 //Define el tipo de mensaje como dato
#define COMMAND 1 //Define el tipo de mensaje como comando
#define APP_MODE 2 //Define el tipo de mensaje como modo APP
bool MODE_SEND_DATA=true;
void writeSerialComWithChecksum(const String &payload);
uint8_t calculateChecksum(const String &data) ;
void serialComInit() {
    Serial.begin(115200);
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

//Este funcion tiene que recivir un String
//Si recive un char* provoca un overflow
void writeSerialComln(String data) {
    writeSerialCom(data + "\n\r");
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

void writeSerialComlnDATA(String data) {
    writeSerialComWithChecksum(String(DATA) + "," + data);
}

void writeSerialComlnCOMMAND(String data) {
    writeSerialComWithChecksum(String(COMMAND) + "," + data);
}

void writeSerialComlnAPP(String data) {

    String payload = String(APP_MODE) + "," + data;
    uint8_t checksum = calculateChecksum(payload);

    char buffer[8];
    sprintf(buffer, "*%02X", checksum);

    String fullLine = payload + String(buffer) + "\r\n";

    Serial.print(fullLine);
}


// Función original para String
void writeSerialCom(String data) {
    
    Serial.print(data);
}

// Sobrecarga para int
void writeSerialCom(int data) {
    Serial.print(data);
}

// Sobrecarga para float
void writeSerialCom(float data) {
    Serial.print(data);
}

// Sobrecarga para double
void writeSerialCom(double data) {
    Serial.print(data);
}

void writeSerialCom(unsigned long data){
    Serial.print(data);
}




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
    sprintf(buffer, "*%02X", checksum);  // 2 dígitos hexadecimales
    Serial.print(payload);
    Serial.print(buffer);
    Serial.print("\n\r");
}