#include "adc.h"
#include "esp_timer.h"
#include <modulos/ina219/ina219.h>
#include <modulos/queueCom/queueCom.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/userInterface/userInterface.h>
#include <modulos/time/time.h>
#include <modulos/carga_electronica/carga_electronica.h>
#include <config.h>
#include <modulos/calibration_manager/calibrationManager.h>

#define ADS_CONVERSION_TIME_US 1200

float lastCurrent_mA[NUMBER_OF_SENSORS]   = {0.0f};
float lastBusVoltage_V[NUMBER_OF_SENSORS] = {0.0f};
float lastPower_mW[NUMBER_OF_SENSORS]     = {0.0f};
static SemaphoreHandle_t currentMutex = nullptr;

bool sensorInUse[NUMBER_OF_SENSORS] = {false};

// ── Recursos para overlap de conversión del ADS1115 ────────────────────────
static esp_timer_handle_t adsTimerHandle;
static SemaphoreHandle_t  adsReadySem;
static volatile uint32_t  adsConversionDoneTimestamp = 0;

static void adsConversionDoneCallback(void* arg) {
    adsConversionDoneTimestamp = customMillis();
    xSemaphoreGive(adsReadySem);
}

void adcInit() {
    pinMode(36, INPUT);
    currentMutex = xSemaphoreCreateMutex();

    adsReadySem = xSemaphoreCreateBinary();

    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = &adsConversionDoneCallback;
    timerArgs.arg      = nullptr;
    timerArgs.name     = "ads_conv_timer";
    esp_timer_create(&timerArgs, &adsTimerHandle);
}

// ── Envío de un sensor ya leído (serial + cola UI + cache para PID) ───────
static void enviarSensor(const ADCData& temp, int i) {
    if (sendDataStatus() == true && getSensorState(i) == true) {
        char buf[96];
        snprintf(buf, sizeof(buf), "%lu,%.3f,%.3f,%.3f,%.3f,%d",
            temp.timestampMillis, temp.current_mA, temp.busVoltage_V,
            temp.shuntVoltage_mV, temp.power_mW, temp.pin);
        writeSerialComlnDATA(buf);
    }
    sendSensorDataToUserInterface(temp);

    if (xSemaphoreTake(currentMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        lastCurrent_mA[i]   = temp.current_mA;
        lastBusVoltage_V[i] = temp.busVoltage_V;
        lastPower_mW[i]     = temp.power_mW;
        xSemaphoreGive(currentMutex);
    }

}



/*
 *
 * El ADS1115 es de conversión única por canal (no hay 2 ADCs en paralelo,
 * datasheet TI SBAS444): cada cambio de mux + conversión tarda ~1.16ms a
 * 860SPS. Como necesitamos 2 canales (tensión y corriente), esperar ambas
 * conversiones de forma bloqueante (delay) sumaría ~2.4ms fijos por ciclo,
 * además de dejar el CPU sin poder atender otras tareas durante la espera.
 *
 * Para evitar eso, cada conversión del ADS1115 se divide en 2 pasos:
 *   - "prepare": dispara la conversión (I2C, no bloqueante)
 *   - "read":    lee el resultado ya convertido
 *
 * y entre el prepare y el read se intercala trabajo útil (leer los 4
 * INA219, o enviar datos ya listos), de modo que la espera de conversión
 * del ADS1115 quede oculta detrás de ese trabajo en vez de sumarse en serie.
 *
 * El aviso de "conversión lista" no se hace con delayMicroseconds (busy-wait
 * que bloquea el scheduler), sino con un esp_timer + semáforo: la tarea se
 * bloquea en xSemaphoreTake() liberando el CPU, y el timer la despierta
 * exactamente cuando termina el tiempo de conversión. El timestamp de cada
 * muestra del ADS1115 se toma en el callback del timer (instante real de
 * fin de conversión), no al momento de leer el registro, para que quede
 * lo más ajustado posible al instante físico de la medición.
 *
 * Orden de ejecución por ciclo:
 *   1) prepare tensión ADS → 2) leer 4x INA219 (oculta la 1ra conversión)
 *   3) leer tensión ADS → 4) prepare corriente ADS → 5) enviar datos ya
 *   listos (oculta la 2da conversión) → 6) leer corriente ADS
 */


void leerADC() {
    ADCData dataArr[NUMBER_OF_SENSORS];
    memset(dataArr, 0, sizeof(dataArr));

    // 1) VCC (A1 vs GND) mientras se leen los 4 INA219
    ads_prepareVcc(dataArr);
    esp_timer_start_once(adsTimerHandle, ADS_CONVERSION_TIME_US);
    leerINA219(dataArr);
    xSemaphoreTake(adsReadySem, pdMS_TO_TICKS(5));
    ads_readVcc(dataArr);

    // 2) Tensión (A2-A3) mientras se envían INA219 0 y 1
    ads_prepareVoltage(dataArr);
    esp_timer_start_once(adsTimerHandle, ADS_CONVERSION_TIME_US);
    enviarSensor(dataArr[0], 0);
    enviarSensor(dataArr[1], 1);
    xSemaphoreTake(adsReadySem, pdMS_TO_TICKS(5));
    ads_readVoltage(dataArr, adsConversionDoneTimestamp);

    // 3) Corriente (A0-A1) mientras se envían INA219 2 y 3
    ads_prepareCurrent(dataArr);
    esp_timer_start_once(adsTimerHandle, ADS_CONVERSION_TIME_US);
    enviarSensor(dataArr[2], 2);
    enviarSensor(dataArr[3], 3);
    xSemaphoreTake(adsReadySem, pdMS_TO_TICKS(5));
    ads_readCurrent(dataArr, adsConversionDoneTimestamp);
    enviarSensor(dataArr[NUMBER_OF_SENSORS - 1], NUMBER_OF_SENSORS - 1);
    float vArr[NUMBER_OF_SENSORS];
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) vArr[i] = dataArr[i].busVoltage_V;
    calibracionTensionUpdate(vArr);
}
static volatile uint32_t mutexFails;
float getLastCurrentData(int i) {
    static float last[NUMBER_OF_SENSORS];
    if (i < 0 || i >= NUMBER_OF_SENSORS) return 0.0f;
    if (xSemaphoreTake(currentMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        last[i] = lastCurrent_mA[i];
        xSemaphoreGive(currentMutex);
    } else mutexFails++;
    return last[i];
}

float getLastBusVoltage(int index) {
    if (index < 0 || index >= NUMBER_OF_SENSORS) return 0.0f;
    float val = 0.0f;
    if (xSemaphoreTake(currentMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        val = lastBusVoltage_V[index];
        xSemaphoreGive(currentMutex);
    }
    return val;
}

float getLastPowerData(int index) {
    if (index < 0 || index >= NUMBER_OF_SENSORS) return 0.0f;
    float val = 0.0f;
    if (xSemaphoreTake(currentMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        val = lastPower_mW[index];
        xSemaphoreGive(currentMutex);
    }
    return val;
}

bool getSensorState(uint8_t index) {
    if (index < NUMBER_OF_SENSORS) return sensorInUse[index];
    return false;
}

bool setSensorState(uint8_t index, bool state) {
    if (index < NUMBER_OF_SENSORS) sensorInUse[index] = state;
    return sensorInUse[index] = state;
}