#ifndef ADCDATA_H
#define ADCDATA_H

// Definición del tipo de datos ADCData
struct ADCData {
    int pin;          // El pin del ADC
    float busVoltage_V;      // El valor medido por el ADC
    float shuntVoltage_mV;
    float current_mA;
    float power_mW;
    unsigned long timestamp;  // Marca de tiempo del dato
};

#endif // ADCDATA_H
