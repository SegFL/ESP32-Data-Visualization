#include "Buffer.h"

// Constructor: inicializa el buffer con un tamaño inicial
Buffer::Buffer(size_t initialSize)
    : buffer(initialSize), currentIndex(0) {}

// Agregar un dato al buffer
void Buffer::addData(const ADCData& data) {
    if (currentIndex >= buffer.size()) {
        throw std::overflow_error("Buffer is full. Consider resizing.");
    }
    buffer[currentIndex++] = data;
}

void Buffer::printData(){

    writeSerialCom("Datos contenidos en el buffer:");
    int i=0;
    
    for(i=0;i<buffer.size();i++){
        ADCData temp=getData(i);
        writeSerialCom(String(temp.pin)+","+String(temp.value));
    }
}

// Obtener un dato del buffer en un índice específico
ADCData Buffer::getData(size_t index) const {
    if (index >= currentIndex) {
        throw std::out_of_range("Index out of range.");
    }
    return buffer[index];
}

// Obtener y eliminar el primer dato del buffer (FIFO)
ADCData Buffer::popData() {
    if (currentIndex == 0) {
        throw std::underflow_error("Buffer is empty.");
    }
    ADCData firstData = buffer[0];

    // Desplazar los datos una posición hacia adelante
    for (size_t i = 1; i < currentIndex; ++i) {
        buffer[i - 1] = buffer[i];
    }

    // Decrementar el índice
    --currentIndex;
    return firstData;
}

// Copiar el primer dato del buffer a un puntero (sin eliminarlo)
void Buffer::copyFirstData(ADCData* dataPtr) {
    if (currentIndex == 0) {
        throw std::underflow_error("Buffer is empty.");
    }
    *dataPtr = buffer[0];  // Copiar el primer dato al puntero
}

// Verificar si el buffer está vacío
bool Buffer::BufferEmpty() const {
    return currentIndex == 0;
}

// Limpiar el buffer
void Buffer::clear() {
    currentIndex = 0;
}

// Aumentar el tamaño del buffer
void Buffer::resize(size_t newSize) {
    if (newSize <= buffer.size()) {
        throw std::invalid_argument("New size must be larger than current size.");
    }
    buffer.resize(newSize);
}

// Obtener el tamaño actual del buffer
size_t Buffer::size() const {
    return buffer.size();
}

// Obtener la cantidad de datos almacenados
size_t Buffer::dataCount() const {
    return currentIndex;
}
