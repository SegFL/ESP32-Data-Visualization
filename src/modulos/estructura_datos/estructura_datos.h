#ifndef ESTRUCTURA_DATOS_H
#define ESTRUCTURA_DATOS_H


#include <modulos/serialCom/serialCom.h>



enum EstadoRegistros {
    WiFiConnected = 0,
    ssid_wifi,
    DIM_DATOS
};


unsigned short read_register( unsigned short reg);
void write_register(unsigned short reg,unsigned short value);
void printStructInfo();
void printDigitalInputs();
void printStructData();
void printRegisterData(unsigned short reg);
void printRegisterInfo(unsigned short reg);
#endif