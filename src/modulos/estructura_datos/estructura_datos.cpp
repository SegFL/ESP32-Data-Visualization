


#include "estructura_datos.h"
#include <stdio.h>



struct{
    unsigned short estructura_datos[DIM_DATOS];
}info_estructura_datos;

void printDataVidente();
// Array con los nombres
static const char* estructura_datos_nombres[DIM_DATOS] = {
    "Estado de WiFi",
    "Red WiFi"


};
//Ojo si esta mal el numero de datos se completa con 0....
static const unsigned char estructura_datos_pos[DIM_DATOS]={
    40,40
};



unsigned short read_register( unsigned short reg){
    
    return info_estructura_datos.estructura_datos[reg];
}


void write_register(unsigned short reg,unsigned short value){
    //Se puede modificar cualquier registro menos el de DIM_DATOS

    //sendUartDatalnint((int)reg);
    if(reg >=DIM_DATOS)
        return;
    
    info_estructura_datos.estructura_datos[reg]=value;
}



void printStructInfo(){
    char buffer[64];

    // Fila 1: titulo

    //Fila 2: Encabesado
    snprintf(buffer, sizeof(buffer), "\033[2;1H%-35s  %s", "Nombre del Registro", "Valor");
    writeSerialCom(buffer);

    // Línea 3 – separador
    snprintf(buffer, sizeof(buffer), "\033[3;1H%-28s  %s", "----------------------------------", "-----");
    writeSerialCom(buffer);
    for (int i = 0; i < DIM_DATOS; i++) {
        printRegisterInfo(i);
    }

}
//Imprime los datos guardados en la estructura
void printStructData(void) {
    for (int i = 0; i < DIM_DATOS; i++) {
        printRegisterData(i);
    }
}



void printRegisterInfo(unsigned short reg) {
    if ( reg >= DIM_DATOS) return;

    char cursorCmd[16];

    // Posicionar el cursor en la fila correspondiente, columna inicial (1)
    snprintf(cursorCmd, sizeof(cursorCmd), "\033[%d;%dH", reg + 4, 1);
    writeSerialCom(cursorCmd);

    // Imprimir el nombre fijo
    writeSerialCom(estructura_datos_nombres[reg]);
}

void printRegisterData(unsigned short reg) {
    if ( reg >= DIM_DATOS) return;

    char buffer[16];
    char cursorCmd[16];

    // Posicionar cursor en la columna de datos
    snprintf(cursorCmd, sizeof(cursorCmd), "\033[%d;%dH", reg + 4, estructura_datos_pos[reg]);
    writeSerialCom(cursorCmd);

    // Borrar valor anterior (opcional)
    writeSerialCom("     ");
    writeSerialComln(cursorCmd);

    // Imprimir nuevo valor
    snprintf(buffer, sizeof(buffer), "%u", info_estructura_datos.estructura_datos[reg]);
    writeSerialCom(buffer);
}