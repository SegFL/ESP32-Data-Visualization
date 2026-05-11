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
const float R_SHUNT_INV[NUMBER_OF_SENSORS] = {
    1.0f / 0.12f,        // 10.0f
    1.0f / 0.0535111f,  // ~18.69f
    1.0f / 0.1f,
    1.0f / 0.1f,
    0.0f
};
float shuntVoltageOffset_mV[NUMBER_OF_SENSORS]={0.1f,0.38f,0.67f,0.67f,0.0f}; // Offset de tensión en mV para cada sensor INA219 (se mide con carga cero)

//ADS1115
float ads_offset_v[NUMBER_OF_SENSORS] = {0.0f, 0.0f, 0.0f,0.0f,  0.0f};
float ads_gain_v[NUMBER_OF_SENSORS]   = {0.0f, 0.0f,0.0f, 0.0f, 14.0f};//13.04

float ads_offset_i[NUMBER_OF_SENSORS];   
float ads_gain_i[NUMBER_OF_SENSORS];
//ACS712

float acs_offset_V[NUMBER_OF_SENSORS]={0.0f,0.0f,0.0f,0.0f,2.5381f};   // ~2.5V + error real medido
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
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS || !sensorAvailable[sensor])
        return false;

    // Puntero local al sensor, el compilador puede mantenerlo en registro
    Adafruit_INA219* const ina = ina219[sensor];

    if (type_sensor[sensor] == INA219_) {
        const float shuntV = ina->getShuntVoltage_mV() - shuntVoltageOffset_mV[sensor];
        const float busV   = ina->getBusVoltage_V();
        const float cur    = shuntV * R_SHUNT_INV[sensor];

        data.shuntVoltage_mV = shuntV;
        data.busVoltage_V    = busV;
        data.current_mA      = cur;
        data.power_mW        = cur * busV;  // reusar variables locales, no campos del struct

    } else { // ADS1115_
        const float cur  = getCalibratedCurrentADS1115(sensor);
        const float busV = getCalibratedVoltageADS1115(sensor);

        data.shuntVoltage_mV = 0.0f;
        data.busVoltage_V    = busV;
        data.current_mA      = cur;
        data.power_mW        = cur * busV;
    }

    data.pin             = sensor;
    data.timestampMillis = customMillis();
    return true;
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