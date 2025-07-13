

#include "serialCom.h"


#define DATA 0 //Define el tipo de mensaje como dato
#define COMMAND 1 //Define el tipo de mensaje como comando

bool MODE_SEND_DATA=false;


void serialComInit() {
    Serial.begin(115200);
}

char readSerialChar() {
    if (Serial.available() > 0) { // Verifica si hay datos disponibles en la terminal serie
        char receivedChar = Serial.read(); // Lee un carácter del buffer serie
        Serial.print(receivedChar); // Loopback: Imprime el carácter recibido (opcional)
        if(receivedChar=='\r'){
            return '\0';    //Filtro los \r pero si los imprimo en pantalla
        }
        return receivedChar; // Retorna el carácter leído
    }
    return '\0'; // Retorna un carácter nulo si no hay datos
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
    writeSerialCom(String(DATA)+String(',')+data + "\n\r");
}

void writeSerialComln(String data) {
    writeSerialCom(String(COMMAND)+String(',')+data + "\n\r");
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