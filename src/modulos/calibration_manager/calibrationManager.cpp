// ════════════════════════════════════════════════════════════════════════════
//  calibrationManager.cpp
//
//  Calibración no bloqueante por interpolación lineal por tramos.
// ════════════════════════════════════════════════════════════════════════════

#include "calibrationManager.h"
#include <Arduino.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Headers del sistema — ajustar paths según tu proyecto
#include <modulos/serialCom/serialCom.h>
#include <modulos/ina219/ina219.h>          // getLastCurrentData, getLastBusVoltage
#include <modulos/carga_electronica/carga_electronica.h>  // setControlMode, setCurrentReference_mA
#include <modulos/pid/pid.h>                // setPIDMode, PID_Init, getDCPID

// Corriente máxima por sensor — debe coincidir con INA219_MAX_CURRENT_A
// y con el rango real del ACS712 para el sensor 4
static const float CALIB_IMAX_mA[NUMBER_OF_SENSORS] = {
    1500.0f,    // sensor 0 — INA219, R=0.12Ω, PGA_4_160MV
    1500.0f,    // sensor 1 — INA219, R=0.10Ω
    5000.0f,    // sensor 2 — INA219, R=0.10Ω
    5000.0f,    // sensor 3 — INA219, R=0.05Ω, PGA_8_320MV deberia ser 6000
    5000.0f,   // sensor 4 — ADS1115 + ACS712 20A Lo modifique poruqe no tengo como caibar algo a 20A
};


// ════════════════════════════════════════════════════════════════════════════
//  Estructura interna de una calibración en curso
//  Espejo exacto del patrón Identificacion_t
// ════════════════════════════════════════════════════════════════════════════
typedef struct {
    // Configuración
    int           sensor;
    int           numPuntos;
    int           muestras_por_punto;   // segundosPorPunto * CALIB_MUESTRAS_POR_SEG
    CalibTipo_t   tipo;

    // Solo para calibración relativa
    int           sensorEntrada;        // sensor de referencia

    // Vectores dinámicos (mismo patrón que Identificacion_t)
    float*        setpoints_mA;         // [numPuntos] corrientes a imponer
    float**       mediciones;           // [numPuntos][muestras_por_punto]
    CalibPar_t*   resultados;           // [numPuntos] pares (measured, real)

    // Estado de avance
    int           puntoActual;
    int           muestraActual;
    int           llamadasTotales;      // para ignorar el standby inicial
    float         valorCongelado_mA;    // promedio calculado, esperando usuario
    CalibEstado_t estado;

    // Estado previo para restaurar al finalizar
    modoFuncionamiento_t prev_modo;
    curve_mode_t         prev_curve;
    bool                 prev_ff;

} Calibracion_t;


// ════════════════════════════════════════════════════════════════════════════
//  Estado global
// ════════════════════════════════════════════════════════════════════════════

// Rutinas en curso (una por sensor)
static Calibracion_t* calibraciones[NUMBER_OF_SENSORS] = {nullptr};

// Tablas de calibración activas — se cargan desde NVS al init
// o se escriben al finalizar una rutina
static CalibTabla_t tablaCorreinte[NUMBER_OF_SENSORS];  // absoluta
static CalibTabla_t tablaRelativa[NUMBER_OF_SENSORS];   // relativa (solo sensor_carga)
static int sensorCargaActivo = -1;  // -1 = ninguno activo
 

// ════════════════════════════════════════════════════════════════════════════
//  Helpers privados
// ════════════════════════════════════════════════════════════════════════════

// Construye la clave NVS igual que makeKey() en carga_electronica
static void makeCalibKey(char* out, size_t outSize,
                          const char* base, int sensor) {
    snprintf(out, outSize, "%s_%d", base, sensor);
}

// Promedio de la segunda mitad del vector — igual que calcularPromedio()
static float promediarSegundaMitad(float* muestras, int total) {
    if (muestras == nullptr || total <= 0) return 0.0f;
    int inicio = total / 2;
    float suma = 0.0f;
    for (int i = inicio; i < total; i++) {
        suma += muestras[i];
    }
    return suma / (float)(total - inicio);
}

// Interpolación lineal por tramos sobre una CalibTabla_t
// Si el valor cae fuera del rango, extrapola con el segmento extremo
static float interpolate(const CalibTabla_t& tabla, float value) {
    const int n = tabla.count;
    if (n <= 0) return value;
    if (n == 1) return tabla.puntos[0].real;

    // Extrapolación por debajo
    if (value <= tabla.puntos[0].measured) {
        float dx = tabla.puntos[1].measured - tabla.puntos[0].measured;
        if (dx == 0.0f) return tabla.puntos[0].real;
        float slope = (tabla.puntos[1].real - tabla.puntos[0].real) / dx;
        return tabla.puntos[0].real + slope * (value - tabla.puntos[0].measured);
    }

    // Extrapolación por encima
    if (value >= tabla.puntos[n-1].measured) {
        float dx = tabla.puntos[n-1].measured - tabla.puntos[n-2].measured;
        if (dx == 0.0f) return tabla.puntos[n-1].real;
        float slope = (tabla.puntos[n-1].real - tabla.puntos[n-2].real) / dx;
        return tabla.puntos[n-1].real + slope * (value - tabla.puntos[n-1].measured);
    }

    // Interpolación dentro del rango
    for (int i = 0; i < n - 1; i++) {
        if (value >= tabla.puntos[i].measured &&
            value <= tabla.puntos[i+1].measured) {
            float dx = tabla.puntos[i+1].measured - tabla.puntos[i].measured;
            if (dx == 0.0f) return tabla.puntos[i].real;
            float t = (value - tabla.puntos[i].measured) / dx;
            return tabla.puntos[i].real + t * (tabla.puntos[i+1].real - tabla.puntos[i].real);
        }
    }

    return value; // no debería llegar acá
}

// Libera toda la memoria dinámica de una calibración en curso
static void liberarCalibracion(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return;
    Calibracion_t* c = calibraciones[sensor];
    if (c == nullptr) return;

    if (c->setpoints_mA != nullptr) {
        free(c->setpoints_mA);
        c->setpoints_mA = nullptr;
    }

    if (c->mediciones != nullptr) {
        for (int i = 0; i < c->numPuntos; i++) {
            if (c->mediciones[i] != nullptr) {
                free(c->mediciones[i]);
                c->mediciones[i] = nullptr;
            }
        }
        free(c->mediciones);
        c->mediciones = nullptr;
    }

    if (c->resultados != nullptr) {
        free(c->resultados);
        c->resultados = nullptr;
    }

    free(c);
    calibraciones[sensor] = nullptr;

    writeSerialComln(String("[CALIB] Memoria liberada — sensor ") + String(sensor));
}

// Restaura el modo de funcionamiento previo a la calibración
static void restaurarModo(int sensor) {
    Calibracion_t* c = calibraciones[sensor];
    if (c == nullptr) return;
    PWMSetCurveMode(c->prev_curve, sensor);
    setControlMode(c->prev_modo, sensor);
    setFeedforwardEnabled(c->prev_ff, sensor);
}

// Calcula los setpoints distribuidos uniformemente entre 0 e I_max
static void calcularSetpoints(float* setpoints, int numPuntos, float imax_mA) {
    if (numPuntos == 1) {
        setpoints[0] = imax_mA;
        return;
    }
    float paso = imax_mA / (float)(numPuntos - 1);
    for (int i = 0; i < numPuntos; i++) {
        setpoints[i] = i * paso;
    }
}

// Impone el setpoint del punto actual vía PID
static void imponerSetpoint(int sensor, float setpoint_mA) {
    setCurrentReference_mA(setpoint_mA, sensor);
}


// ════════════════════════════════════════════════════════════════════════════
//  Procesado final y guardado en NVS — equivalente a procesarYGuardar()
// ════════════════════════════════════════════════════════════════════════════

static void procesarYGuardarAbsoluta(int sensor) {
    Calibracion_t* c = calibraciones[sensor];
    if (c == nullptr) return;

    // Copiar resultados a la tabla activa
    int n = c->numPuntos;
    tablaCorreinte[sensor].count  = n;
    tablaCorreinte[sensor].activa = true;
    for (int i = 0; i < n; i++) {
        tablaCorreinte[sensor].puntos[i] = c->resultados[i];
    }

    // Guardar en NVS
    guardarCalibracionAbsoluta(sensor);

    // Imprimir resumen — con heap aún válido
    writeSerialComln(String("[CALIB] === CALIBRACIÓN ABSOLUTA SENSOR ") + String(sensor) + String(" ==="));
    for (int i = 0; i < n; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "  [%d] sensor=%.2f mA  real=%.2f mA",
                 i,
                 c->resultados[i].measured,
                 c->resultados[i].real);
        writeSerialComln(String(buf));
    }
    writeSerialComln(String("[CALIB] === FIN ==="));

    setCalibracionFlag(sensor, true);
    restaurarModo(sensor);
    setCurrentReference_mA(0.0f, sensor);  // ← apagar la carga al terminar
    liberarCalibracion(sensor);
}

static void procesarYGuardarRelativa(int sensorCarga) {
    Calibracion_t* c = calibraciones[sensorCarga];
    if (c == nullptr) return;
 
    int n = c->numPuntos;
 
    // La tabla relativa se indexa por sensorCarga pero se aplica al sensor 4
    tablaRelativa[sensorCarga].count  = n;
    tablaRelativa[sensorCarga].activa = true;
    for (int i = 0; i < n; i++) {
        tablaRelativa[sensorCarga].puntos[i] = c->resultados[i];
    }
 
    guardarCalibracionRelativa(sensorCarga);
 
    writeSerialComln(String("[CALIB] === CALIBRACIÓN RELATIVA sensor4 vs sensor") +
                    String(sensorCarga) + String(" ==="));
    for (int i = 0; i < n; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "  [%d] sensor4=%.2f mA  sensor%d=%.2f mA",
                 i,
                 c->resultados[i].measured,
                 sensorCarga,
                 c->resultados[i].real);
        writeSerialComln(String(buf));
    }
    writeSerialComln(String("[CALIB] === FIN ==="));
 
    restaurarModo(sensorCarga);
    setCurrentReference_mA(0.0f, sensorCarga);
    liberarCalibracion(sensorCarga);
}

// ════════════════════════════════════════════════════════════════════════════
//  Alocación y configuración común a absoluta y relativa
// ════════════════════════════════════════════════════════════════════════════

static Calibracion_t* allocCalibracion(int sensor, int numPuntos, int segundosPorPunto) {
    Calibracion_t* c = (Calibracion_t*)malloc(sizeof(Calibracion_t));
    if (c == nullptr) {
        writeSerialComln(String("[CALIB] Sin memoria para estructura"));
        return nullptr;
    }
    memset(c, 0, sizeof(Calibracion_t));

    c->sensor             = sensor;
    c->numPuntos          = numPuntos;
    c->muestras_por_punto = segundosPorPunto * CALIB_MUESTRAS_POR_SEG;
    c->puntoActual        = 0;
    c->muestraActual      = 0;
    c->llamadasTotales    = 0;
    c->valorCongelado_mA  = 0.0f;
    c->estado             = CALIB_ACUMULANDO;

    // Setpoints
    c->setpoints_mA = (float*)malloc(sizeof(float) * numPuntos);
    if (c->setpoints_mA == nullptr) {
        free(c);
        writeSerialComln(String("[CALIB] Sin memoria para setpoints"));
        return nullptr;
    }
    calcularSetpoints(c->setpoints_mA, numPuntos, CALIB_IMAX_mA[sensor]);

    // Matriz de mediciones
    c->mediciones = (float**)malloc(sizeof(float*) * numPuntos);
    if (c->mediciones == nullptr) {
        free(c->setpoints_mA);
        free(c);
        writeSerialComln(String("[CALIB] Sin memoria para mediciones[]"));
        return nullptr;
    }
    memset(c->mediciones, 0, sizeof(float*) * numPuntos);

    for (int i = 0; i < numPuntos; i++) {
        c->mediciones[i] = (float*)malloc(sizeof(float) * c->muestras_por_punto);
        if (c->mediciones[i] == nullptr) {
            for (int j = 0; j < i; j++) free(c->mediciones[j]);
            free(c->mediciones);
            free(c->setpoints_mA);
            free(c);
            writeSerialComln(String("[CALIB] Sin memoria para mediciones[") + String(i) + String("]"));
            return nullptr;
        }
        memset(c->mediciones[i], 0, sizeof(float) * c->muestras_por_punto);
    }

    // Vector de resultados
    c->resultados = (CalibPar_t*)malloc(sizeof(CalibPar_t) * numPuntos);
    if (c->resultados == nullptr) {
        for (int i = 0; i < numPuntos; i++) free(c->mediciones[i]);
        free(c->mediciones);
        free(c->setpoints_mA);
        free(c);
        writeSerialComln(String("[CALIB] Sin memoria para resultados"));
        return nullptr;
    }
    memset(c->resultados, 0, sizeof(CalibPar_t) * numPuntos);

    return c;
}


// ════════════════════════════════════════════════════════════════════════════
//  API pública — Init
// ════════════════════════════════════════════════════════════════════════════

void calibrationManagerInit() {
    // Inicializar tablas a identidad (sin corrección)
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        memset(&tablaCorreinte[i], 0, sizeof(CalibTabla_t));
        memset(&tablaRelativa[i],  0, sizeof(CalibTabla_t));
        calibraciones[i] = nullptr;

        // Intentar cargar desde NVS
        cargarCalibracionNVS(i);
        imprimirCalibracionAbsoluta(i);
    }
    writeSerialComln(String("[CALIB] calibrationManager inicializado"));
}


// ════════════════════════════════════════════════════════════════════════════
//  Calibración absoluta — Init
// ════════════════════════════════════════════════════════════════════════════

bool calibracionAbsolutaInit(int sensor, int numPuntos, int segundosPorPunto) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) {
        writeSerialComln(String("[CALIB] sensor invalido"));
        return false;
    }
    if (numPuntos < 2 || numPuntos > CALIB_MAX_POINTS) {
        writeSerialComln(String("[CALIB] numPuntos debe estar entre 2 y ") + String(CALIB_MAX_POINTS));
        return false;
    }
    if (segundosPorPunto <= 0 || segundosPorPunto > 60) {
        writeSerialComln(String("[CALIB] segundosPorPunto invalido"));
        return false;
    }

    // Si ya había una calibración activa, abortarla
    if (calibraciones[sensor] != nullptr) {
        calibracionAbortar(sensor);
    }

    Calibracion_t* c = allocCalibracion(sensor, numPuntos, segundosPorPunto);
    if (c == nullptr) return false;

    c->tipo        = CALIB_ABSOLUTA;
    c->sensorEntrada = -1;

    // Guardar estado previo
    c->prev_modo  = getModoFuncionamiento(sensor);
    c->prev_curve = getCurveMode(sensor);
    bool ff;
    getFeedforwardEnabled(&ff, sensor);
    c->prev_ff = ff;

    // Configurar canal: PID en modo corriente, FF desactivado
    // (el feedforward se supone ya desactivado, pero lo forzamos igual)
    setFeedforwardEnabled(false, sensor);
    PWMSetCurveMode(OFF_t, sensor);
    setControlMode(PID, sensor);
    setPIDMode(sensor, 'i');

    calibraciones[sensor] = c;
    //Para calibrar uso los datos crudos del sensor
    setCalibracionFlag(sensor, false);  

    // Imponer primer setpoint
    imponerSetpoint(sensor, c->setpoints_mA[0]);

    writeSerialComln(
        String("[CALIB] Calibración absoluta iniciada — sensor=") + String(sensor) +
        String(" puntos=") + String(numPuntos) +
        String(" seg/punto=") + String(segundosPorPunto) +
        String(" I_max=") + String(CALIB_IMAX_mA[sensor]) + String(" mA")
    );

    return true;
}


// ════════════════════════════════════════════════════════════════════════════
//  Calibración relativa — Init
// ════════════════════════════════════════════════════════════════════════════

bool calibracionRelativaInit(int sensorEntrada, int sensorCarga,
                              int numPuntos, int segundosPorPunto) {
    if (sensorEntrada < 0 || sensorEntrada >= NUMBER_OF_SENSORS ||
        sensorCarga   < 0 || sensorCarga   >= NUMBER_OF_SENSORS ||
        sensorEntrada == sensorCarga) {
        writeSerialComln(String("[CALIB] Sensores invalidos para calibración relativa"));
        return false;
    }
    if (numPuntos < 2 || numPuntos > CALIB_MAX_POINTS) {
        writeSerialComln(String("[CALIB] numPuntos invalido"));
        return false;
    }
    if (segundosPorPunto <= 0 || segundosPorPunto > 60) {
        writeSerialComln(String("[CALIB] segundosPorPunto invalido"));
        return false;
    }

    if (calibraciones[sensorCarga] != nullptr) {
        calibracionAbortar(sensorCarga);
    }

    // Para relativa, los setpoints se basan en el rango del sensor de carga
    Calibracion_t* c = allocCalibracion(sensorCarga, numPuntos, segundosPorPunto);
    if (c == nullptr) return false;

    c->tipo          = CALIB_RELATIVA;
    c->sensorEntrada = sensorEntrada;

    // Guardar estado previo del sensor de CARGA
    c->prev_modo  = getModoFuncionamiento(sensorCarga);
    c->prev_curve = getCurveMode(sensorCarga);
    bool ff;
    getFeedforwardEnabled(&ff, sensorCarga);
    c->prev_ff = ff;

    setFeedforwardEnabled(false, sensorCarga);
    PWMSetCurveMode(OFF_t, sensorCarga);
    setControlMode(PID, sensorCarga);
    setPIDMode(sensorCarga, 'i');

    calibraciones[sensorCarga] = c;

    imponerSetpoint(sensorCarga, c->setpoints_mA[0]);

    writeSerialComln(
        String("[CALIB] Calibración relativa iniciada — entrada=") + String(sensorEntrada) +
        String(" carga=") + String(sensorCarga) +
        String(" puntos=") + String(numPuntos)
    );

    return true;
}


// ════════════════════════════════════════════════════════════════════════════
//  calibracionUpdate — llamar desde CargaElectronicaUpdate() cada tick
//
//  Equivalente exacto de guardarPuntoIdentificado()
// ════════════════════════════════════════════════════════════════════════════

void calibracionUpdate(int sensor, float muestra_mA) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return;
    Calibracion_t* c = calibraciones[sensor];
    if (c == nullptr || c->estado == CALIB_IDLE) return;

    // Si estamos esperando al usuario no acumular muestras
    if (c->estado == CALIB_ESPERANDO_USUARIO) return;

    // Ignorar las llamadas del standby inicial de estabilización
    int llamadasStandby = CALIB_STANDBY_S * CALIB_MUESTRAS_POR_SEG;
    if (c->llamadasTotales < llamadasStandby) {
        c->llamadasTotales++;
        return;
    }

    // Acumular muestra
    if (c->puntoActual < c->numPuntos &&
        c->muestraActual < c->muestras_por_punto) {
        c->mediciones[c->puntoActual][c->muestraActual] = muestra_mA;
        c->muestraActual++;
    }
    c->llamadasTotales++;

    // ¿Completamos las muestras del punto actual?
    if (c->muestraActual >= c->muestras_por_punto) {

        // Calcular promedio de la segunda mitad (igual que calcularPromedio())
        c->valorCongelado_mA = promediarSegundaMitad(
            c->mediciones[c->puntoActual],
            c->muestras_por_punto
        );

        if (c->tipo == CALIB_ABSOLUTA) {
            // Pedir valor al usuario
            c->estado = CALIB_ESPERANDO_USUARIO;
            writeSerialComln(
                String("[CALIB] Punto ") + String(c->puntoActual) +
                String("/") + String(c->numPuntos - 1) +
                String(" | Sensor midió: ") + String(c->valorCongelado_mA, 2) +
                String(" mA | Ingresá el valor real (mA):")
            );

        } else {
            // Calibración relativa: tomar automáticamente el valor del sensor de entrada
            float valorEntrada = getLastCurrentData(c->sensorEntrada);

            // Aplicar la calibración absoluta del sensor de entrada si está disponible
            if (tablaCorreinte[c->sensorEntrada].activa) {
                valorEntrada = interpolate(tablaCorreinte[c->sensorEntrada], valorEntrada);
            }

            c->resultados[c->puntoActual] = {
                .measured = c->valorCongelado_mA,
                .real     = valorEntrada
            };

            writeSerialComln(
                String("[CALIB REL] Punto ") + String(c->puntoActual) +
                String(" | carga=") + String(c->valorCongelado_mA, 2) +
                String(" mA | entrada=") + String(valorEntrada, 2) + String(" mA")
            );

            c->puntoActual++;
            c->muestraActual  = 0;
            c->llamadasTotales = 0; // resetear standby para el próximo punto

            if (c->puntoActual >= c->numPuntos) {
                c->estado = CALIB_FINALIZANDO;
                procesarYGuardarRelativa(sensor);
            } else {
                // Avanzar al siguiente setpoint
                imponerSetpoint(sensor, c->setpoints_mA[c->puntoActual]);
            }
        }
    }
}


// ════════════════════════════════════════════════════════════════════════════
//  calibracionRecibirValorReal — llamar desde el parser UART
// ════════════════════════════════════════════════════════════════════════════

bool calibracionRecibirValorReal(int sensor, float valorReal_mA) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return false;

    Calibracion_t* c = calibraciones[sensor];
    if (c == nullptr || c->estado != CALIB_ESPERANDO_USUARIO) {
        writeSerialComln(String("[CALIB] No hay calibración esperando valor en sensor ") + String(sensor));
        return false;
    }
    if (valorReal_mA < 0.0f) {
        writeSerialComln(String("[CALIB] Valor real inválido"));
        return false;
    }

    // Guardar el par
    c->resultados[c->puntoActual] = {
        .measured = c->valorCongelado_mA,
        .real     = valorReal_mA
    };

    writeSerialComln(
        String("[CALIB] Par guardado [") + String(c->puntoActual) + String("]") +
        String(" sensor=") + String(c->valorCongelado_mA, 2) +
        String(" real=")   + String(valorReal_mA, 2)
    );

    c->puntoActual++;
    c->muestraActual   = 0;
    c->llamadasTotales = 0; // resetear standby para el próximo punto
    c->estado          = CALIB_ACUMULANDO;

    if (c->puntoActual >= c->numPuntos) {
        // Último punto completado
        c->estado = CALIB_FINALIZANDO;
        procesarYGuardarAbsoluta(sensor);
    } else {
        // Avanzar al siguiente setpoint
        imponerSetpoint(sensor, c->setpoints_mA[c->puntoActual]);
        writeSerialComln(
            String("[CALIB] Avanzando al punto ") + String(c->puntoActual) +
            String(" — setpoint: ") + String(c->setpoints_mA[c->puntoActual], 1) + String(" mA")
        );
    }

    return true;
}


// ════════════════════════════════════════════════════════════════════════════
//  Abortar
// ════════════════════════════════════════════════════════════════════════════

void calibracionAbortar(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return;
    if (calibraciones[sensor] == nullptr) return;

    writeSerialComln(String("[CALIB] Abortando calibración sensor ") + String(sensor));
    restaurarModo(sensor);
    liberarCalibracion(sensor);
}


// ════════════════════════════════════════════════════════════════════════════
//  NVS — guardar y cargar
// ════════════════════════════════════════════════════════════════════════════

bool guardarCalibracionAbsoluta(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return false;

    CalibTabla_t* t = &tablaCorreinte[sensor];
    nvs_handle_t handle;

    if (nvs_open(CALIB_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        writeSerialComln(String("[CALIB] Error al abrir NVS para escritura absoluta"));
        return false;
    }

    char key[32];
    makeCalibKey(key, sizeof(key), "abs_npts", sensor);
    nvs_set_i32(handle, key, t->count);

    makeCalibKey(key, sizeof(key), "abs_data", sensor);
    nvs_set_blob(handle, key, t->puntos, sizeof(CalibPar_t) * t->count);

    makeCalibKey(key, sizeof(key), "abs_flag", sensor);
    nvs_set_i32(handle, key, (int32_t)t->activa);

    nvs_commit(handle);
    nvs_close(handle);

    writeSerialComln(String("[CALIB] Calibración absoluta guardada — sensor ") + String(sensor));
    return true;
}

bool guardarCalibracionRelativa(int sensorCarga) {
    if (sensorCarga < 0 || sensorCarga >= NUMBER_OF_SENSORS) return false;

    CalibTabla_t* t = &tablaRelativa[sensorCarga];
    nvs_handle_t handle;

    if (nvs_open(CALIB_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        writeSerialComln(String("[CALIB] Error al abrir NVS para escritura relativa"));
        return false;
    }

    char key[32];
    makeCalibKey(key, sizeof(key), "rel_npts", sensorCarga);
    nvs_set_i32(handle, key, t->count);

    makeCalibKey(key, sizeof(key), "rel_data", sensorCarga);
    nvs_set_blob(handle, key, t->puntos, sizeof(CalibPar_t) * t->count);

    makeCalibKey(key, sizeof(key), "rel_flag", sensorCarga);
    nvs_set_i32(handle, key, (int32_t)t->activa);

    nvs_commit(handle);
    nvs_close(handle);

    writeSerialComln(String("[CALIB] Calibración relativa guardada — sensor ") + String(sensorCarga));
    return true;
}

bool cargarCalibracionNVS(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return false;

    nvs_handle_t handle;
    if (nvs_open(CALIB_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false; // namespace vacío, es normal al primer arranque
    }

    char key[32];
    bool cargadaAlguna = false;

    // ── Absoluta ──────────────────────────────────────────────────────────
    {
        int32_t npts = 0;
        makeCalibKey(key, sizeof(key), "abs_npts", sensor);
        if (nvs_get_i32(handle, key, &npts) == ESP_OK &&
            npts > 0 && npts <= CALIB_MAX_POINTS) {

            size_t blobSize = sizeof(CalibPar_t) * npts;
            makeCalibKey(key, sizeof(key), "abs_data", sensor);
            if (nvs_get_blob(handle, key, tablaCorreinte[sensor].puntos, &blobSize) == ESP_OK) {
                tablaCorreinte[sensor].count = npts;

                int32_t flag = 1;
                makeCalibKey(key, sizeof(key), "abs_flag", sensor);
                nvs_get_i32(handle, key, &flag);
                tablaCorreinte[sensor].activa = (bool)flag;

                writeSerialComln(
                    String("[CALIB] Calibración absoluta cargada — sensor ") + String(sensor) +
                    String(" | ") + String(npts) + String(" puntos")
                );
                cargadaAlguna = true;
            }
        }
    }

    // ── Relativa ──────────────────────────────────────────────────────────
    {
        int32_t npts = 0;
        makeCalibKey(key, sizeof(key), "rel_npts", sensor);
        if (nvs_get_i32(handle, key, &npts) == ESP_OK &&
            npts > 0 && npts <= CALIB_MAX_POINTS) {

            size_t blobSize = sizeof(CalibPar_t) * npts;
            makeCalibKey(key, sizeof(key), "rel_data", sensor);
            if (nvs_get_blob(handle, key, tablaRelativa[sensor].puntos, &blobSize) == ESP_OK) {
                tablaRelativa[sensor].count = npts;

                int32_t flag = 1;
                makeCalibKey(key, sizeof(key), "rel_flag", sensor);
                nvs_get_i32(handle, key, &flag);
                tablaRelativa[sensor].activa = (bool)flag;

                writeSerialComln(
                    String("[CALIB] Calibración relativa cargada — sensor ") + String(sensor) +
                    String(" | ") + String(npts) + String(" puntos")
                );
                cargadaAlguna = true;
            }
        }
    }

    nvs_close(handle);
    return cargadaAlguna;
}


// ════════════════════════════════════════════════════════════════════════════
//  Aplicación de calibración — llamar desde getData()
//
//  Se aplica primero la calibración absoluta y luego la relativa (si existe).
//  Esto respeta el orden: absoluta fija la referencia, relativa ajusta la
//  diferencia entre sensores.
// ════════════════════════════════════════════════════════════════════════════

float applyCurrentCalib(int sensor, float current_mA) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return current_mA;
 
    float result = current_mA;
 
    // Calibración absoluta — aplica a todos los sensores
    if (tablaCorreinte[sensor].activa && tablaCorreinte[sensor].count > 0) {
        result = interpolate(tablaCorreinte[sensor], result);
    }
 
    // Calibración relativa — solo aplica al sensor 4 (sensor de entrada)
    // Usa la tabla correspondiente al sensor de carga activo en este momento
    if (sensor == 4 && sensorCargaActivo >= 0 && sensorCargaActivo < NUMBER_OF_SENSORS) {
        if (tablaRelativa[sensorCargaActivo].activa &&
            tablaRelativa[sensorCargaActivo].count > 0) {
            result = interpolate(tablaRelativa[sensorCargaActivo], result);
        }
    }
 
    return result;
}

float applyVoltageCalib(int sensor, float voltage_V) {
    // Por ahora solo calibración absoluta de tensión
    // (la relativa de tensión se puede agregar con otra tabla si se necesita)
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return voltage_V;
    // Placeholder: si en el futuro se agrega tablaVoltaje[], se interpola acá
    return voltage_V;
}


// ════════════════════════════════════════════════════════════════════════════
//  Estado y diagnóstico
// ════════════════════════════════════════════════════════════════════════════

bool calibracionActiva(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return false;
    return (calibraciones[sensor] != nullptr &&
            calibraciones[sensor]->estado != CALIB_IDLE);
}

bool calibracionDisponible(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return false;
    return (tablaCorreinte[sensor].activa && tablaCorreinte[sensor].count > 0);
}

void setCalibracionFlag(int sensor, bool activa) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return;
    tablaCorreinte[sensor].activa = activa;
    tablaRelativa[sensor].activa  = activa;
}

bool getCalibracionFlag(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return false;
    return tablaCorreinte[sensor].activa;
}

CalibEstado_t getCalibEstado(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return CALIB_IDLE;
    if (calibraciones[sensor] == nullptr) return CALIB_IDLE;
    return calibraciones[sensor]->estado;
}

void imprimirCalibracionAbsoluta(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return;
    CalibTabla_t* t = &tablaCorreinte[sensor];

    writeSerialComln(String("[CALIB] === Calibración absoluta sensor ") + String(sensor) + String(" ==="));
    writeSerialComln(String("  Activa: ") + String(t->activa ? "SI" : "NO"));
    writeSerialComln(String("  Puntos: ") + String(t->count));
    for (int i = 0; i < t->count; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "  [%d] sensor=%.2f mA  real=%.2f mA",
                 i, t->puntos[i].measured, t->puntos[i].real);
        writeSerialComln(String(buf));
    }
    writeSerialComln(String("[CALIB] ==================================="));
}

void imprimirCalibracionRelativa(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return;
    CalibTabla_t* t = &tablaRelativa[sensor];

    writeSerialComln(String("[CALIB] === Calibración relativa sensor ") + String(sensor) + String(" ==="));
    writeSerialComln(String("  Activa: ") + String(t->activa ? "SI" : "NO"));
    writeSerialComln(String("  Puntos: ") + String(t->count));
    for (int i = 0; i < t->count; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "  [%d] carga=%.2f mA  entrada=%.2f mA",
                 i, t->puntos[i].measured, t->puntos[i].real);
        writeSerialComln(String(buf));
    }
    writeSerialComln(String("[CALIB] ==================================="));
}

void imprimirCalibracionTodas() {
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        imprimirCalibracionAbsoluta(i);
        imprimirCalibracionRelativa(i);
    }
}


void setSensorCargaActivo(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) {
        sensorCargaActivo = -1;
        return;
    }
    sensorCargaActivo = sensor;
}
 
int getSensorCargaActivo() {
    return sensorCargaActivo;
}
 
float applyCurrentCalibAbsOnly(int sensor, float current_mA) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return current_mA;
    if (tablaCorreinte[sensor].activa && tablaCorreinte[sensor].count > 0) {
        return interpolate(tablaCorreinte[sensor], current_mA);
    }
    return current_mA;
}



// En calibrationManager.cpp agregar:
void setAbsoluteCalibFlag(int sensor, bool activa) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return;
    tablaCorreinte[sensor].activa = activa;
}

bool getAbsoluteCalibFlag(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return false;
    return tablaCorreinte[sensor].activa;
}

void setRelativeCalibFlag(int sensor, bool activa) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return;
    tablaRelativa[sensor].activa = activa;
}

bool getRelativeCalibFlag(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return false;
    return tablaRelativa[sensor].activa;
}
