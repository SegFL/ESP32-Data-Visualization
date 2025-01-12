// Buffer.h
#ifndef BUFFER_H
#define BUFFER_H

#include <stdexcept>
#include "ADCData.h"
#include <modulos/serialCom/serialCom.h>

#define MAX_SIZE 100

class Buffer {
public:
    Buffer();

    // Agregar un dato al buffer
    void addData(const ADCData& data);

    // Obtener y eliminar el primer dato del buffer (FIFO)
    ADCData popData();

    // Obtener el primer dato del buffer sin eliminarlo
    ADCData peekData() const;

    // Verificar si el buffer está vacío
    bool isEmpty() const;

    // Verificar si el buffer está lleno
    bool isFull() const;

    // Limpiar el buffer
    void clear();

    // Imprimir los datos almacenados
    void printData() const;

    // Obtener la cantidad de elementos almacenados
    size_t size() const;

private:
    ADCData buffer[MAX_SIZE];
    size_t front; // Índice del primer elemento
    size_t rear;  // Índice del último elemento + 1
    size_t count; // Cantidad de elementos almacenados
};

#endif // BUFFER_H
