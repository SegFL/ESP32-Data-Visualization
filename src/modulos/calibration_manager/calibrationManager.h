#pragma once
#include <stdint.h>
#include <stdbool.h>

// ════════════════════════════════════════════════════════════════════════════
//  calibrationManager.h
//
//  Módulo de calibración por interpolación lineal por tramos.
//  Soporta calibración absoluta (sensor vs instrumento externo) y
//  calibración relativa (sensor_entrada vs sensor_carga sin DUT).
//
//  Lógica no bloqueante: calibracionUpdate() se llama desde
//  CargaElectronicaUpdate() en cada tick, igual que guardarPuntoIdentificado.
// ════════════════════════════════════════════════════════════════════════════

#define CALIB_MAX_POINTS        10      // máximo de puntos por tabla
#define CALIB_NVS_NAMESPACE     "calib" // namespace NVS dedicado
#define CALIB_MUESTRAS_POR_SEG  10      // debe coincidir con la frecuencia de leerADC()
#define CALIB_STANDBY_S         3       // segundos de estabilización antes de acumular


// ── Tipos de calibración ──────────────────────────────────────────────────
typedef enum {
    CALIB_IDLE,             // sin rutina activa
    CALIB_ACUMULANDO,       // acumulando muestras del punto actual
    CALIB_ESPERANDO_USUARIO,// muestras congeladas, esperando input UART
    CALIB_FINALIZANDO       // procesando y guardando
} CalibEstado_t;

typedef enum {
    CALIB_ABSOLUTA,
    CALIB_RELATIVA
} CalibTipo_t;

// ── Par de calibración ────────────────────────────────────────────────────
typedef struct {
    float measured;     // lo que leyó el sensor
    float real;         // lo que midió el instrumento externo (o sensor_entrada)
} CalibPar_t;

// ── Tabla de calibración (corriente o tensión) ────────────────────────────
typedef struct {
    CalibPar_t puntos[CALIB_MAX_POINTS];
    int        count;
    bool       activa;  // true = se aplica en applyCurrentCalib / applyVoltageCalib
} CalibTabla_t;


// ════════════════════════════════════════════════════════════════════════════
//  API pública
// ════════════════════════════════════════════════════════════════════════════

// ── Init del módulo (llamar una vez en setup) ─────────────────────────────
void calibrationManagerInit();

// ── Calibración absoluta ──────────────────────────────────────────────────

// Inicia la rutina de calibración absoluta de corriente para un sensor.
// Configura PID en modo corriente, distribuye numPuntos setpoints entre
// 0 y I_max del sensor, y arranca la acumulación de muestras.
// segundosPorPunto: tiempo de estabilización + acumulación por punto.
bool calibracionAbsolutaInit(int sensor, int numPuntos, int segundosPorPunto);

// Llamar desde CargaElectronicaUpdate() en cada tick.
// Acumula muestras; al completar un punto congela el promedio y espera al usuario.
void calibracionUpdate(int sensor, float muestra_mA);

// El parser UART llama a esta función cuando el usuario ingresa el valor real.
// Guarda el par (valorCongelado, valorReal), avanza al siguiente punto o finaliza.
bool calibracionRecibirValorReal(int sensor, float valorReal_mA);

// Aborta la rutina en curso, restaura el modo previo y libera memoria.
void calibracionAbortar(int sensor);


// ── Calibración relativa ──────────────────────────────────────────────────

// Inicia calibración relativa entre sensor_entrada y sensor_carga.
// No bloqueante: usa el mismo mecanismo de calibracionUpdate().
// El usuario NO ingresa valores: los toma automáticamente del sensor_entrada.
bool calibracionRelativaInit(int sensorEntrada, int sensorCarga,
                              int numPuntos, int segundosPorPunto);


// ── NVS ──────────────────────────────────────────────────────────────────
bool guardarCalibracionAbsoluta(int sensor);
bool guardarCalibracionRelativa(int sensorCarga);
bool cargarCalibracionNVS(int sensor);     // carga absoluta + relativa si existen


// ── Aplicación de calibración (llamar desde getData()) ───────────────────
float applyCurrentCalib(int sensor, float current_mA);
float applyVoltageCalib(int sensor, float voltage_V);


// ── Estado y diagnóstico ──────────────────────────────────────────────────
bool calibracionActiva(int sensor);                 // rutina en curso
bool calibracionDisponible(int sensor);             // tabla cargada y activa
void setCalibracionFlag(int sensor, bool activa);   // activar/desactivar aplicación
bool getCalibracionFlag(int sensor);
CalibEstado_t getCalibEstado(int sensor);

void imprimirCalibracionAbsoluta(int sensor);
void imprimirCalibracionRelativa(int sensor);
void imprimirCalibracionTodas();
float applyCurrentCalibAbsOnly(int sensor, float current_mA);

void  setAbsoluteCalibFlag(int sensor, bool activa);
bool  getAbsoluteCalibFlag(int sensor);
void  setRelativeCalibFlag(int sensor, bool activa);
bool  getRelativeCalibFlag(int sensor);


