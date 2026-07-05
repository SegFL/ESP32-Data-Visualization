#include "ina219.h"
#include <modulos/serialCom/serialCom.h>
#include <ADCData.h>
#include "ADS1X15.h"
#include <Wire.h>
#include <modulos/calibration_manager/calibrationManager.h>


typedef enum {
    INA219_,
    ADS1115_
} sensor_type_t;

sensor_type_t type_sensor[NUMBER_OF_SENSORS] = {INA219_, INA219_,INA219_,INA219_, ADS1115_};


// ── Calibracion  ───────────────────────
//INA219
const float R_SHUNT_INV[NUMBER_OF_SENSORS] = {
    9.463566f,       // ← era 1.0f/0.12f = 8.333. Corrige ganancia (slope=1.1356)
    1.0f / 0.1f,
    1.0f / 0.1f,
    1.0f / 0.05f,
    0.0f
};

float shuntVoltageOffset_mV[NUMBER_OF_SENSORS] = {
    -0.6474f,        // ← era 0.1f. Negativo porque internamente se resta
    0.38f,
    0.67f,
    0.67f,
    0.0f
};

// ── Calibración interna del registro INA219 ───────────────────────────────
// Corriente máxima esperada por sensor (A). Determina la resolución del Cal register.
// Ajustar según el circuito: I_max_posible = Vshunt_max / R_shunt
//   Sensor 0: 0.12Ω → 160mV/0.12 = 1.33A  → usamos 1.5A con margen
//   Sensor 1: 0.10Ω → 160mV/0.10 = 1.60A  → usamos 1.5A
//   Sensor 2: 0.10Ω → 160mV/0.10 = 1.60A  → usamos 1.5A
//   Sensor 3: 0.05Ω → 160mV/0.05 = 3.20A  → usamos 3.0A
//   Sensor 4: ADS1115, no aplica
static const float INA219_MAX_CURRENT_A[NUMBER_OF_SENSORS] = {1.5f, 1.5f, 5.0f, 5.0f, 0.0f};

// Valores calculados en ina219Init() y reutilizados en cada lectura
static uint16_t ina219_cal_value[NUMBER_OF_SENSORS]  = {0};
static float    ina219_lsb_mA[NUMBER_OF_SENSORS]     = {0.0f};
static uint16_t ina219_config_reg[NUMBER_OF_SENSORS] = {0};

// ── Registros INA219 (datasheet tabla 4) ─────────────────────────────────
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNTVOLTAGE 0x01
#define INA219_REG_BUSVOLTAGE   0x02
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

// Bits de configuración
#define INA219_CONFIG_BRNG_32V         ((uint16_t)(1 << 13))
#define INA219_CONFIG_PGA_4_160MV      ((uint16_t)(0b10 << 11)) // ±160mV: cubre todos los shunts
#define INA219_CONFIG_BADC_12BIT       ((uint16_t)(0b0011 << 7))
#define INA219_CONFIG_SADC_12BIT       ((uint16_t)(0b0011 << 3))
#define INA219_CONFIG_MODE_CONT        ((uint16_t)0b111)         // shunt+bus continuo

//ADS1115
float ads_offset_v[NUMBER_OF_SENSORS] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
float ads_gain_v[NUMBER_OF_SENSORS]   = {0.0f, 0.0f, 0.0f, 0.0f, 14.0f};

float ads_offset_i[NUMBER_OF_SENSORS];
float ads_gain_i[NUMBER_OF_SENSORS];

//ACS712
float acs_offset_V[NUMBER_OF_SENSORS] = {0.0f, 0.0f, 0.0f, 0.0f,  1.6255f}; //Si l alimentacion es de 3.3V, el offset es 1.65V. Si la alimentacion es de 5V, el offset es 2.5V
float acs_gain[NUMBER_OF_SENSORS]     = {1.0f, 1.0f, 1.0f, 1.0f, (5/3.3)*(1.0f+0.0101+2.50/100.0f)*10000.0f};//La sensibilidad tambien depende de VCC



// ── Variables globales ────────────────────────────────────────────────────
ADS1115* ads1115[NUMBER_OF_SENSORS];
// Adafruit_INA219* ina219[NUMBER_OF_SENSORS];  // reemplazado por acceso directo
uint8_t sensorAddresses[NUMBER_OF_SENSORS] = {0x40, 0x41, 0x44, 0x45, 0x48};
bool sensorAvailable[NUMBER_OF_SENSORS]    = {false, false, false, false, false};


float getCalibratedCurrentADS1115(int sensor);
float getCalibratedVoltageADS1115(int sensor);

// ── Helpers I2C crudo ─────────────────────────────────────────────────────
static bool ina219_writeReg(uint8_t addr, uint8_t reg, uint16_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xFF));
    return Wire.endTransmission() == 0;
}

static int16_t ina219_readReg(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0;
    Wire.requestFrom((uint8_t)addr, (uint8_t)2);
    if (Wire.available() < 2) return 0;
    return (int16_t)(((uint16_t)Wire.read() << 8) | Wire.read());
}

// ── Init ──────────────────────────────────────────────────────────────────
void ina219Init() {
    writeSerialComln(String("Inicializando sensores INA219 y ADS1115..."));

    Wire.begin();
    Wire.setClock(400000);

    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {

        if (type_sensor[i] == INA219_) {

            // Verificar que el sensor responde
            Wire.beginTransmission(sensorAddresses[i]);
            if (Wire.endTransmission() != 0) {
                writeSerialCom("Error al inicializar INA219 en 0x");
                writeSerialComln(String(sensorAddresses[i], HEX));
                sensorAvailable[i] = false;
                continue;
            }

            // Calcular Cal register: Cal = trunc(0.04096 / (LSB_A * R_shunt))
            // LSB_A = max_current / 32767, redondeado al µA superior
            float lsb_A = INA219_MAX_CURRENT_A[i] / 32767.0f;
            lsb_A = ceilf(lsb_A * 1e6f) * 1e-6f;   // redondear al µA entero superior
            ina219_lsb_mA[i]    = lsb_A * 1000.0f;

            uint16_t cal = (uint16_t)(0.04096f / (lsb_A * (1.0f / R_SHUNT_INV[i])));
            cal &= 0xFFFE;  // bit D0 es void (datasheet sección 8.6), siempre 0
            ina219_cal_value[i] = cal;
            uint16_t pga = (i == 3 ||i==4) ? ((uint16_t)(0b11 << 11))   // PGA_8 → ±320mV → 6.4A máx
                                    : ((uint16_t)(0b10 << 11));   // PGA_4 → ±160mV → resto


            // Registro de configuración
            ina219_config_reg[i] = INA219_CONFIG_BRNG_32V
                                 | pga
                                 | INA219_CONFIG_BADC_12BIT
                                 | INA219_CONFIG_SADC_12BIT
                                 | INA219_CONFIG_MODE_CONT;

            // Reset del chip (bit 15 = 1) y escritura de Config + Cal
            ina219_writeReg(sensorAddresses[i], INA219_REG_CONFIG, 0x8000);
            delayMicroseconds(100);
            ina219_writeReg(sensorAddresses[i], INA219_REG_CONFIG,      ina219_config_reg[i]);
            ina219_writeReg(sensorAddresses[i], INA219_REG_CALIBRATION, ina219_cal_value[i]);

            sensorAvailable[i] = true;
            writeSerialCom("INA219 en 0x");
            writeSerialCom(String(sensorAddresses[i], HEX));
            writeSerialComln(String(" listo."));

        } else if (type_sensor[i] == ADS1115_) {
            ads1115[i] = new ADS1115(sensorAddresses[i], &Wire);
            if (!ads1115[i]->begin()) {
                writeSerialCom("Error al inicializar ADS1115 en 0x");
                writeSerialComln(String(sensorAddresses[i], HEX));
                sensorAvailable[i] = false;
                continue;
            }
            ads1115[i]->setGain(1);
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

    if (type_sensor[sensor] == INA219_) {

        // Reescritura defensiva del Cal register: protege ante reset por transitorio
        ina219_writeReg(sensorAddresses[sensor], INA219_REG_CALIBRATION, ina219_cal_value[sensor]);

        // Shunt Voltage Register: signed 16-bit, LSB = 10µV
        const int16_t raw_shunt   = ina219_readReg(sensorAddresses[sensor], INA219_REG_SHUNTVOLTAGE);
        // Bus Voltage Register: bits 15:3 = valor, bit 0 = OVF — desplazar 3 bits
        const int16_t raw_bus     = ina219_readReg(sensorAddresses[sensor], INA219_REG_BUSVOLTAGE);
        // Current Register: signed 16-bit, LSB = ina219_lsb_mA[sensor]
        const int16_t raw_current = ina219_readReg(sensorAddresses[sensor], INA219_REG_CURRENT);

        const float shuntV = (raw_shunt * 10.0f / 1000.0f) - shuntVoltageOffset_mV[sensor]; // 10µV/bit → mV
        const float cur  = applyCurrentCalib(sensor, (float)raw_current * ina219_lsb_mA[sensor]);
        const float busV = applyVoltageCalib(sensor, (float)(raw_bus >> 3) * 4.0f / 1000.0f);
        data.shuntVoltage_mV = shuntV;
        data.busVoltage_V    = busV;
        data.current_mA      = cur;
        data.power_mW        = cur * busV;

    } else { // ADS1115_
    const float rawCur     = getCalibratedCurrentADS1115(sensor);
    const float afterAbs   = applyCurrentCalibAbsOnly(sensor, rawCur);
    const float cur        = applyCurrentCalib(sensor, rawCur);
    const float busV       = applyVoltageCalib(sensor, getCalibratedVoltageADS1115(sensor));

    char buf[128];
    snprintf(buf, sizeof(buf),
        "[S4] raw=%.2f  +absoluta=%.2f  +relativa=%.2f mA",
        rawCur, afterAbs, cur);
    //writeSerialComln(String(buf));

    data.shuntVoltage_mV = 0.0f;
    data.busVoltage_V    = busV;
    data.current_mA      = cur;
    data.power_mW        = cur * busV;

    }

    data.pin             = sensor;
    data.timestampMillis = customMillis();
    return true;
}


// Como el ADS1115 no se calibra, lo hago a mano
float getCalibratedVoltageADS1115(int sensor) {
    float raw       = ads1115[sensor]->readADC_Differential_2_3();
    float corrected = raw - ads_offset_v[sensor];
    float voltage   = ads1115[sensor]->toVoltage(corrected);
    voltage *= ads_gain_v[sensor];
    return voltage;
}

float getCalibratedCurrentADS1115(int sensor) {
    float raw     = ads1115[sensor]->readADC_Differential_0_1();
    float voltage = ads1115[sensor]->toVoltage(raw);
    float deltaV  = voltage - acs_offset_V[sensor];
    float current = deltaV * acs_gain[sensor];


    return current;
}


/*

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
    1.0f / 0.1f,  // ~18.69f
    1.0f / 0.1f,
    1.0f / 0.05f,
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

*/