
#include "userInterface.h"
#define MAX_DATA_BUFFER 30
#define ESC 27
static MenuNode *menu = nullptr;
String data_buffer = ""; //Variable para almacenar los datos recibidos
bool aceptandoDatos=false;

void moveCursor(int row, int col);
void procesarDatos(String data);

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
    writeSerialComln("Procesando datos");
    if(menu->id==3){//Decido a que modulo/funcion le mando los datos que entraron por pantalla
        writeSerialComln("Llame a la funcion cargar contraseña");
    }

}
