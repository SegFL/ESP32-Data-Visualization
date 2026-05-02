#include "ina219.h"
#include <modulos/serialCom/serialCom.h>
#include <ADCData.h>
#include "ADS1X15.h"



typedef enum {
    INA219_,
    ADS1115_
} sensor_type_t;

sensor_type_t type_sensor[NUMBER_OF_SENSORS] = {INA219_, INA219_,INA219_,INA219_, ADS1115_}; // Configura el tipo de cada sensor


// ── Calibracion  ───────────────────────
//INA219
const float R_SHUNT_OHMS[NUMBER_OF_SENSORS] = {
    0.1f,         // Sensor 0x40 (INA219)
    0.0535111f,   // Sensor 0x41 (INA219)
    0.1f,         // Sensor 0x48 (ADS1115) - no se usa
    0.1f          // Sensor 0x49 (ADS1115) - no se usa
};
float shuntVoltageOffset_mV[NUMBER_OF_SENSORS]={0.67f,0.38f,0.67f,0.67f,0.0f}; // Offset de tensión en mV para cada sensor INA219 (se mide con carga cero)

//ADS1115
float ads_offset_v[NUMBER_OF_SENSORS] = {0.0f, 0.0f, 0.0f,0.0f,  0.0f};
float ads_gain_v[NUMBER_OF_SENSORS]   = {0.0f, 0.0f,0.0f, 0.0f, 13.04f};

float ads_offset_i[NUMBER_OF_SENSORS];   
float ads_gain_i[NUMBER_OF_SENSORS];
//ACS712

float acs_offset_V[NUMBER_OF_SENSORS]={0.0f,0.0f,0.0f,0.0f,2.5f + 0.068f};   // ~2.5V + error real medido
//En realidad la ganancia seria el valor normal, pero se invierte para evitar dividir por 0
float acs_gain[NUMBER_OF_SENSORS]={1.0f,1.0f,1.0f,1.0f,10000.0f};       // V/A (ej: 0.100 para 100mV/A*1000 para mA/A)


// ── Variables globales ────────────────────────────────────────────────────
ADS1115* ads1115[NUMBER_OF_SENSORS]; // Sensores ADS1115 
Adafruit_INA219* ina219[NUMBER_OF_SENSORS];  // ← CAMBIO: era [2], ahora [NUMBER_OF_SENSORS]
uint8_t sensorAddresses[NUMBER_OF_SENSORS] = {0x40, 0x41,0x44, 0x45, 0x48};  // Direcciones I2C de los sensores
bool sensorAvailable[NUMBER_OF_SENSORS] = {false, false, false,false, false};




float getCalibratedCurrentADS1115(int sensor);
float getCalibratedVoltageADS1115(int sensor) ;
// ── Init ──────────────────────────────────────────────────────────────────
void ina219Init() {
    writeSerialComln(String("Inicializando sensores INA219 y ADS1115..."));

    //Inicio el bus I2C antes de inicializar los sensores para poder elejir a que frecuencia funciona
    Wire.begin();
    Wire.setClock(400000);  

    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {


        if(type_sensor[i] == INA219_) {

            ina219[i] = new Adafruit_INA219(sensorAddresses[i]);
            
            if (!ina219[i]->begin()) {
                writeSerialCom("Error al inicializar INA219 en 0x");
                writeSerialComln(String(sensorAddresses[i], HEX));
                sensorAvailable[i] = false;
                continue;
            }
            
            sensorAvailable[i] = true;
            writeSerialCom("INA219 en 0x");
            writeSerialCom(String(sensorAddresses[i], HEX));
            writeSerialComln(String(" listo."));
            
        } else if(type_sensor[i] == ADS1115_) {
            ads1115[i] = new ADS1115(sensorAddresses[i], &Wire);
            if (!ads1115[i]->begin()) {   // depende de la librería, algunas usan begin()
                    writeSerialCom("Error al inicializar ADS1115 en 0x");
                    writeSerialComln(String(sensorAddresses[i], HEX));
                    sensorAvailable[i] = false;
                    continue;
                }
                ads1115[i]->setGain(1); // ±4.096V, para medir hasta 3.3V con margen
                sensorAvailable[i] = true;
                writeSerialCom("ADS1115 en 0x");
                writeSerialCom(String(sensorAddresses[i], HEX));
                writeSerialComln(" listo.");

        }
    }
}

// ── Lectura de datos ──────────────────────────────────────────────────────
bool getData(ADCData& data, int const sensor) {
    if (sensor >= 0 && sensor < NUMBER_OF_SENSORS && sensorAvailable[sensor] == true) {


        if(type_sensor[sensor] == INA219_) {

            // getBusVoltage_V() y getShuntVoltage_mV() no tocan el registro
            // de calibración → valores directos del ADC, siempre confiables
            data.busVoltage_V    = ina219[sensor]->getBusVoltage_V();
            data.shuntVoltage_mV = ina219[sensor]->getShuntVoltage_mV()-shuntVoltageOffset_mV[sensor]; // Restar offset de shunt

            // Corriente y potencia calculadas manualmente
            // I = V_shunt / R_shunt
            data.current_mA = data.shuntVoltage_mV / R_SHUNT_OHMS[sensor];
            data.power_mW   = data.current_mA * data.busVoltage_V;
            
        } else if(type_sensor[sensor] == ADS1115_) {
            float val_cur = getCalibratedCurrentADS1115(sensor);
            float val_bus = getCalibratedVoltageADS1115(sensor);
            //Asigno la tension y corriente a la estructura como si fuese un INA219 
            data.busVoltage_V   = val_bus;
            data.shuntVoltage_mV = 0.0f;
            data.current_mA      = val_cur;
            data.power_mW        = data.current_mA * data.busVoltage_V;
        }

        data.pin             = sensor;
        data.timestampMillis = customMillis();
        writeSerialComln(String(data.timestampMillis));
        return true;
    }
    return false;
}


//Como el ADS1115 no se calibra,lo hago a mano
float getCalibratedVoltageADS1115(int sensor) {

    float raw = ads1115[sensor]->readADC_Differential_2_3();

    float corrected = raw - ads_offset_v[sensor];

    float voltage = ads1115[sensor]->toVoltage(corrected);

    voltage *= ads_gain_v[sensor];

    // corregir divisor resistivo
    //voltage /= 0.076621;

    return voltage;
}
float getCalibratedCurrentADS1115(int sensor) {

    float raw = ads1115[sensor]->readADC_Differential_0_1();
    // Paso 1: convertir directo a voltaje
    float voltage = ads1115[sensor]->toVoltage(raw);

    // Paso 2: restar offset del ACS712 (~2.5V real)
    float deltaV = voltage - acs_offset_V[sensor];
    //  PROTECCIÓN

    // Paso 3: convertir a corriente (V/A)
    float current = deltaV * acs_gain[sensor]; 

    return current; 
}