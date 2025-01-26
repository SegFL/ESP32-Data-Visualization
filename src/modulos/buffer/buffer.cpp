// Buffer.cpp
#include "buffer.h"

Buffer::Buffer()
    : front(0), rear(0), count(0) {}

void Buffer::addData(const ADCData& data) {
    if (isFull()) {
        throw std::overflow_error("Buffer is full.");
    }
    buffer[rear] = data;
    rear = (rear + 1) % MAX_SIZE;
    ++count;
}

ADCData Buffer::popData() {
    if (isEmpty()) {
        throw std::underflow_error("Buffer is empty.");
    }
    ADCData data = buffer[front];
    front = (front + 1) % MAX_SIZE;
    --count;
    return data;
}

ADCData Buffer::peekData() const {
    if (isEmpty()) {
        throw std::underflow_error("Buffer is empty.");
    }
    return buffer[front];
}

bool Buffer::isEmpty() const {
    return count == 0;
}

bool Buffer::isFull() const {
    return count == MAX_SIZE;
}

void Buffer::clear() {
    front = 0;
    rear = 0;
    count = 0;
}

void Buffer::printData() const {
    writeSerialCom("Datos contenidos en el buffer:");
    size_t idx = front;
    for (size_t i = 0; i < count; ++i) {
        const ADCData& data = buffer[idx];
        writeSerialCom("Pin: " + String(data.pin) + ", Value: " + String(data.busVoltage_V) + ", Timestamp: " + String(data.timestamp));
        idx = (idx + 1) % MAX_SIZE;
    }
}

size_t Buffer::size() const {
    return count;
}
