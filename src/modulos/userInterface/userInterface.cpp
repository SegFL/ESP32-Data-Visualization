
#include "userInterface.h"
#include <ADCData.h>
#include "modulos/queueCom/queueCom.h"
#define MAX_DATA_BUFFER 30
#define ESC 27
static MenuNode *menu = nullptr;
String data_buffer = ""; //Variable para almacenar los datos recibidos
bool aceptandoDatos=false;

void moveCursor(int row, int col);
void procesarDatos(String data);
void printSensorData();

void userInterfaceInit(){
    serialComInit();
    clearScreen();//Borra mensajes del ESP32 al iniciar el programa
    menu=menuInit();
    if(menu){
        writeSerialComln("Menu inicializado");
    }else{
        writeSerialComln("Menu no inicializado");
    }
}


void userInterfaceUpdate(){
    char charReceived = readSerialChar();

    if(charReceived=='\0'||menu==nullptr){
        return;
    }

    if(charReceived=='\n'){//Si se presiona enter se cambia el estado de recvir datos
        if(aceptandoDatos==true){
            aceptandoDatos=false;
            writeSerialComln("Datos recibidos");
            writeSerialComln(String(data_buffer));
            procesarDatos(data_buffer);

            data_buffer="";
        }else{
            aceptandoDatos=true;
            data_buffer="";
        }
        return;
    }

    if(aceptandoDatos==false){
        menuUpdate(charReceived, &menu);
        clearScreen();
        printNode(menu);
        if(menu->id==1){
            writeSerialComln("Precesando datos del sensor");
            printSensorData();

        }
    }else{
        //writeSerialComln("Guarde:"+String(charReceived));
        data_buffer += charReceived;  // Agregarlo al buffer
    }


    return;
}



void procesarDatos(String data){
    if(data==""||data==nullptr||menu==nullptr){
        return;
    }
    if(menu->id==3){//Decido a que modulo/funcion le mando los datos que entraron por pantalla
        writeSerialComln("Llame a la funcion cargar contraseña");
    }
    if(menu->id==1){
        writeSerialComln("Datos de sensor");
        printSensorData();
    }
    

}



void printSensorData() {
    //ADCData data;
    ADCData data = {A0, 5.0, 10.0, 2.5, 12.5, millis()};

    if(receiveSensorDataToUserInterface(data)==false){
        return;
    }
 
    writeSerialComln(String("Pin: ") + String(data.pin));
    writeSerialComln(String("\tBus Voltage: ") + String(data.busVoltage_V) + String(" V"));
    writeSerialComln(String("\tShunt Voltage: ") + String(data.shuntVoltage_mV) + String(" mV"));
    writeSerialComln(String("\tCurrent: ") + String(data.current_mA) + String(" mA"));
    writeSerialComln(String("\tPower: ") + String(data.power_mW) + String(" mW"));
    writeSerialComln(String("\tTimestamp: ") + String(data.timestamp));

}


