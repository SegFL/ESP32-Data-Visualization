#include "ina219.h"
#include <modulos/serialCom/serialCom.h>
#include <ADCData.h>

// ── Resistencias shunt reales por sensor (en Ohms) ───────────────────────
// Ajustá cada valor si medís diferente con multímetro
const float R_SHUNT_OHMS[4] = {
    0.12282f,   // Sensor 0x40
    0.12495f,   // Sensor 0x41
    0.1f,   // Sensor 0x44
    0.1f    // Sensor 0x45
};

// ── Variables globales ────────────────────────────────────────────────────
Adafruit_INA219* ina219[NUMBER_OF_SENSORS];
uint8_t sensorAddresses[4] = {0x40, 0x41, 0x44, 0x45};
bool sensorAvailable[4]    = {false, false, false, false};

// ── Init ──────────────────────────────────────────────────────────────────
void ina219Init() {
    writeSerialComln(String("Inicializando sensores INA219..."));

    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        ina219[i] = new Adafruit_INA219(sensorAddresses[i]);

        if (!ina219[i]->begin()) {
            writeSerialCom("Error al inicializar INA219 en 0x");
            writeSerialComln(String(sensorAddresses[i], HEX));
            sensorAvailable[i] = false;
            continue;
        }

        // begin() llama internamente a setCalibration_32V_2A()
        // que configura el modo correcto (32V, ganancia /8, 12bit, continuo)
        // No necesitamos hacer nada más — la corriente la calculamos
        // manualmente desde shuntVoltage, sin usar el registro de calibración

        sensorAvailable[i] = true;
        writeSerialCom("INA219 en 0x");
        writeSerialCom(String(sensorAddresses[i], HEX));
        writeSerialComln(String(" listo."));
    }
}

// ── Lectura de datos ──────────────────────────────────────────────────────
bool getData(ADCData& data, int sensor) {
    if (sensor < NUMBER_OF_SENSORS && sensorAvailable[sensor] == true) {

        // getBusVoltage_V() y getShuntVoltage_mV() no tocan el registro
        // de calibración → valores directos del ADC, siempre confiables
        data.busVoltage_V    = ina219[sensor]->getBusVoltage_V();
        data.shuntVoltage_mV = ina219[sensor]->getShuntVoltage_mV();

        // Corriente y potencia calculadas manualmente
        // I = V_shunt / R_shunt
        data.current_mA = data.shuntVoltage_mV / R_SHUNT_OHMS[sensor];
        data.power_mW   = data.current_mA * data.busVoltage_V;

        data.pin             = sensor;
        data.timestampMillis = customMillis();
        return true;
    }
    return false;
}