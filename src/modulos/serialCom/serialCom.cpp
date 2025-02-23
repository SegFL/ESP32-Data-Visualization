

#include "serialCom.h"

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

void writeSerialComln(String data) {
    writeSerialCom(data + "\n\r");
}

void writeSerialCom(String data) {
    Serial.print(data);
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
