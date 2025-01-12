#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <stdexcept>
#include "ADCData.h"
#include <modulos/serialCom/serialCom.h>


class Buffer {
private:
    std::vector<ADCData> buffer;  // Contenedor dinámico para almacenar datos
    size_t currentIndex;          // Índice actual para agregar datos

public:
    // Constructor: inicializa el buffer con un tamaño inicial
    explicit Buffer(size_t initialSize);

    // Agregar un dato al buffer
    void addData(const ADCData& data);

    void printData();

    // Obtener un dato del buffer en un índice específico
    ADCData getData(size_t index) const;

    // Obtener y eliminar el primer dato del buffer (FIFO)
    ADCData popData();

    // Copiar el primer dato del buffer a un puntero (sin eliminarlo)
    void copyFirstData(ADCData* dataPtr);

    // Verificar si el buffer está vacío
    bool BufferEmpty() const;

    // Limpiar el buffer
    void clear();

    // Aumentar el tamaño del buffer
    void resize(size_t newSize);

    // Obtener el tamaño actual del buffer
    size_t size() const;

    // Obtener la cantidad de datos almacenados
    size_t dataCount() const;
};

#endif  // BUFFER_H
