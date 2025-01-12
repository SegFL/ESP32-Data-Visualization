#ifndef ADCDATA_H
#define ADCDATA_H

// Definición del tipo de datos ADCData
struct ADCData {
    int pin;          // El pin del ADC
    int value;      // El valor medido por el ADC
    unsigned long timestamp;  // Marca de tiempo del dato
};

#endif // ADCDATA_H
