#include <Arduino.h>
#include <HardwareSerial.h>




void serialComInit();
char readSerialChar();
void writeSerialCom(String data);
void writeSerialComln(String data);

int serialComAvailable();

void clearScreen();

void serialComInit();
char readSerialChar();
void writeSerialComln(String data);
void writeSerialComlnDATA(String data);
//Cambia elmodo de funcionameinto del sistmea
//TRUE: Envia los datos de los sensores por la interfaz serie
//FALSE: El sistema funciona normalmente como menu
void changeMode(bool mode);
//Vevuelve TRUE si esta en modo send data
bool sendDataStatus();


// Función original para String
void writeSerialCom(String data);

// Sobrecarga para int
void writeSerialCom(int data);

// Sobrecarga para float
void writeSerialCom(float data);

// Sobrecarga para double
void writeSerialCom(double data);

void writeSerialCom(unsigned long data);





