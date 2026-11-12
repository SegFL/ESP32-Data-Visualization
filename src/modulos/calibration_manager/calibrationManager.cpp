// ════════════════════════════════════════════════════════════════════════════
//  calibrationManager.cpp
//
//  Calibración no bloqueante por interpolación lineal por tramos.
//
//  Convención de la calibración RELATIVA:
//    - El sensor que se corrige es SIEMPRE el CALIB_SENSOR_REL (4: ACS712+ADS1115).
//    - La referencia es un INA219 (sensorRef), que además es el canal que
//      impone la corriente por PID durante la rutina.
//    - Hay una tabla relativa por cada INA219 (tablaRelativa[sensorRef]),
//      pero solo una se aplica a la vez (sensorCargaActivo = sensorRef activo).
//    - Par guardado: measured = lectura del sensor 4, real = lectura del INA219.
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

// Sensor que se corrige en la calibración relativa (ACS712 + ADS1115)
#define CALIB_SENSOR_REL 4

// Clave NVS de la referencia relativa activa
#define CALIB_NVS_KEY_REF "rel_ref"

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
    int           sensor;               // sensor que impone la carga (absoluta: el calibrado;
                                        // relativa: el INA219 de referencia)
    int           numPuntos;
    int           muestras_por_punto;   // segundosPorPunto * CALIB_MUESTRAS_POR_SEG
    CalibTipo_t   tipo;

    // Solo para calibración relativa
    double        sumaS4;               // acumulador del sensor 4 (2ª mitad del punto)
    int           nS4;                  // cantidad de muestras acumuladas
    int           prev_refActivo;       // sensorCargaActivo previo, para restaurar

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
static CalibTabla_t tablaRelativa[NUMBER_OF_SENSORS];   // relativa, indexada por INA219 de referencia
static int sensorCargaActivo = -1;  // INA219 cuya tabla relativa se aplica al sensor 4; -1 = ninguno


// ════════════════════════════════════════════════════════════════════════════
//  Helpers privados
// ════════════════════════════════════════════════════════════════════════════

// Construye la clave NVS igual que makeKey() en carga_electronica
static void makeCalibKey(char* out, size_t outSize,
                          const char* base, int sensor) {
    snprintf(out, outSize, "%s_%d", base, sensor);
}

// Persiste cuál es la referencia relativa activa (-1 = ninguna)
static void persistirRefActiva(int ref) {
    nvs_handle_t h;
    if (nvs_open(CALIB_NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, CALIB_NVS_KEY_REF, (int32_t)ref);
        nvs_commit(h);
        nvs_close(h);
    }
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
// (y, en la relativa, la referencia activa que había antes de empezar)
static void restaurarModo(int sensor) {
    Calibracion_t* c = calibraciones[sensor];
    if (c == nullptr) return;
    PWMSetCurveMode(c->prev_curve, sensor);
    setControlMode(c->prev_modo, sensor);
    setFeedforwardEnabled(c->prev_ff, sensor);
    if (c->tipo == CALIB_RELATIVA) {
        sensorCargaActivo = c->prev_refActivo;
    }
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

    // Las tablas relativas se calcularon sobre las absolutas vigentes en ese
    // momento. Si cambia la base, quedan inválidas.
    if (sensor == CALIB_SENSOR_REL) {
        // Cambió la base del sensor 4: todas las relativas pierden validez
        for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
            if (tablaRelativa[i].activa) {
                tablaRelativa[i].activa = false;
                if (tablaRelativa[i].count > 0) guardarCalibracionRelativa(i);
            }
        }
        sensorCargaActivo = -1;
        persistirRefActiva(-1);
        writeSerialComln(String("[CALIB] Absoluta del sensor 4 cambió: relativas invalidadas"));
    } else if (tablaRelativa[sensor].activa) {
        // Cambió la referencia 'sensor': solo su tabla pierde validez
        tablaRelativa[sensor].activa = false;
        if (tablaRelativa[sensor].count > 0) guardarCalibracionRelativa(sensor);
        if (sensorCargaActivo == sensor) {
            sensorCargaActivo = -1;
            persistirRefActiva(-1);
        }
        writeSerialComln(String("[CALIB] Absoluta del sensor ") + String(sensor) +
                         String(" cambió: su relativa fue invalidada"));
    }

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

    setAbsoluteCalibFlag(sensor, true);   // no toca la flag de la relativa
    restaurarModo(sensor);
    setCurrentReference_mA(0.0f, sensor);  // ← apagar la carga al terminar
    liberarCalibracion(sensor);
}

static void procesarYGuardarRelativa(int sensorRef) {
    Calibracion_t* c = calibraciones[sensorRef];
    if (c == nullptr) return;

    int n = c->numPuntos;

    // Tabla indexada por la referencia (INA219); se aplica al sensor 4
    tablaRelativa[sensorRef].count  = n;
    tablaRelativa[sensorRef].activa = true;
    for (int i = 0; i < n; i++) {
        tablaRelativa[sensorRef].puntos[i] = c->resultados[i];
    }

    guardarCalibracionRelativa(sensorRef);

    writeSerialComln(String("[CALIB] === CALIBRACIÓN RELATIVA sensor") + String(CALIB_SENSOR_REL) +
                     String(" vs ref INA219 ") + String(sensorRef) + String(" ==="));
    for (int i = 0; i < n; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "  [%d] sensor%d=%.2f mA  ref%d=%.2f mA",
                 i,
                 CALIB_SENSOR_REL,
                 c->resultados[i].measured,
                 sensorRef,
                 c->resultados[i].real);
        writeSerialComln(String(buf));
    }
    writeSerialComln(String("[CALIB] === FIN ==="));

    // restaurarModo devuelve la referencia activa previa: calibrar la
    // referencia B no desactiva la A que se venía usando. Para activar la
    // recién calibrada, usar setSensorCargaActivo(sensorRef).
    restaurarModo(sensorRef);
    setCurrentReference_mA(0.0f, sensorRef);
    liberarCalibracion(sensorRef);
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
    c->sumaS4             = 0.0;
    c->nS4                = 0;
    c->prev_refActivo     = -1;
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

    // Restaurar la referencia relativa que estaba activa
    sensorCargaActivo = -1;
    {
        nvs_handle_t h;
        int32_t ref = -1;
        if (nvs_open(CALIB_NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
            if (nvs_get_i32(h, CALIB_NVS_KEY_REF, &ref) != ESP_OK) ref = -1;
            nvs_close(h);
        }
        if (ref >= 0 && ref < NUMBER_OF_SENSORS && ref != CALIB_SENSOR_REL &&
            tablaRelativa[ref].activa && tablaRelativa[ref].count > 0) {
            sensorCargaActivo = (int)ref;
        }
    }

    writeSerialComln(String("[CALIB] calibrationManager inicializado | ref relativa activa: ") +
                     String(sensorCargaActivo));
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

    c->tipo = CALIB_ABSOLUTA;

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
    // Para calibrar uso los datos crudos del sensor (solo se apaga la absoluta;
    // la relativa de este sensor no se toca)
    setAbsoluteCalibFlag(sensor, false);

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
//
//  sensor 4 (ACS712+ADS1115) vs un INA219 de referencia (sensorRef).
//  Los dos deben estar en serie (sin DUT en el medio) para que circule la
//  misma corriente por ambos. Las absolutas de ambos deben estar vigentes.
// ════════════════════════════════════════════════════════════════════════════

bool calibracionRelativaInit(int sensorRef, int numPuntos, int segundosPorPunto) {
    if (sensorRef < 0 || sensorRef >= NUMBER_OF_SENSORS || sensorRef == CALIB_SENSOR_REL) {
        writeSerialComln(String("[CALIB] Referencia invalida (debe ser un INA219)"));
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

    if (calibraciones[sensorRef] != nullptr) {
        calibracionAbortar(sensorRef);
    }

    // Los setpoints se basan en el rango del INA219 de referencia (canal de carga)
    Calibracion_t* c = allocCalibracion(sensorRef, numPuntos, segundosPorPunto);
    if (c == nullptr) return false;

    c->tipo = CALIB_RELATIVA;

    // Guardar estado previo del canal de carga y de la referencia activa
    c->prev_modo      = getModoFuncionamiento(sensorRef);
    c->prev_curve     = getCurveMode(sensorRef);
    bool ff;
    getFeedforwardEnabled(&ff, sensorRef);
    c->prev_ff        = ff;
    c->prev_refActivo = sensorCargaActivo;

    // Apagar la relativa mientras se calibra: el sensor 4 debe leerse solo
    // con su absoluta, si no "measured" se contamina con una tabla vieja.
    // Se restaura en restaurarModo() (fin o aborto).
    sensorCargaActivo = -1;

    setFeedforwardEnabled(false, sensorRef);
    PWMSetCurveMode(OFF_t, sensorRef);
    setControlMode(PID, sensorRef);
    setPIDMode(sensorRef, 'i');

    calibraciones[sensorRef] = c;

    imponerSetpoint(sensorRef, c->setpoints_mA[0]);

    writeSerialComln(
        String("[CALIB] Calibración relativa iniciada — sensor") + String(CALIB_SENSOR_REL) +
        String(" vs ref INA219 ") + String(sensorRef) +
        String(" puntos=") + String(numPuntos) +
        String(" seg/punto=") + String(segundosPorPunto) +
        String(" I_max=") + String(CALIB_IMAX_mA[sensorRef]) + String(" mA")
    );

    return true;
}


// ════════════════════════════════════════════════════════════════════════════
//  calibracionUpdate — llamar desde CargaElectronicaUpdate() cada tick
//
//  'sensor' es el canal que impone la carga y 'muestra_mA' su lectura.
//  En la relativa ese canal es el INA219 de referencia.
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

        // Relativa: acumular el sensor 4 en la misma ventana (2ª mitad) que
        // usa promediarSegundaMitad() para la referencia
        if (c->tipo == CALIB_RELATIVA &&
            c->muestraActual >= c->muestras_por_punto / 2) {
            c->sumaS4 += (double)getLastCurrentData(CALIB_SENSOR_REL);
            c->nS4++;
        }

        c->muestraActual++;
    }
    c->llamadasTotales++;

    // ¿Completamos las muestras del punto actual?
    if (c->muestraActual >= c->muestras_por_punto) {

        // Promedio de la segunda mitad (igual que calcularPromedio())
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
            // Relativa: referencia (INA219, con su absoluta) vs sensor 4 (solo absoluta)
            float ref = c->valorCongelado_mA;
            float s4  = (c->nS4 > 0) ? (float)(c->sumaS4 / (double)c->nS4) : 0.0f;

            c->resultados[c->puntoActual] = {
                .measured = s4,     // lo que lee el sensor que se corrige
                .real     = ref     // lo que lee la referencia
            };

            writeSerialComln(
                String("[CALIB REL] Punto ") + String(c->puntoActual) +
                String(" | sensor4=") + String(s4, 2) +
                String(" mA | ref=") + String(ref, 2) + String(" mA")
            );

            c->puntoActual++;
            c->muestraActual   = 0;
            c->llamadasTotales = 0;     // resetear standby para el próximo punto
            c->sumaS4          = 0.0;
            c->nS4             = 0;

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
//  calibracionRecibirValorReal — llamar desde el parser UART (solo absoluta)
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
    restaurarModo(sensor);              // en relativa también restaura la ref activa
    setCurrentReference_mA(0.0f, sensor);
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

bool guardarCalibracionRelativa(int sensorRef) {
    if (sensorRef < 0 || sensorRef >= NUMBER_OF_SENSORS) return false;

    CalibTabla_t* t = &tablaRelativa[sensorRef];
    nvs_handle_t handle;

    if (nvs_open(CALIB_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        writeSerialComln(String("[CALIB] Error al abrir NVS para escritura relativa"));
        return false;
    }

    char key[32];
    makeCalibKey(key, sizeof(key), "rel_npts", sensorRef);
    nvs_set_i32(handle, key, t->count);

    makeCalibKey(key, sizeof(key), "rel_data", sensorRef);
    nvs_set_blob(handle, key, t->puntos, sizeof(CalibPar_t) * t->count);

    makeCalibKey(key, sizeof(key), "rel_flag", sensorRef);
    nvs_set_i32(handle, key, (int32_t)t->activa);

    nvs_commit(handle);
    nvs_close(handle);

    writeSerialComln(String("[CALIB] Calibración relativa guardada — ref ") + String(sensorRef));
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
                    String("[CALIB] Calibración relativa cargada — ref ") + String(sensor) +
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
//  Se aplica primero la calibración absoluta y luego, solo al sensor 4, la
//  relativa de la referencia seleccionada (sensorCargaActivo).
//  Orden: la absoluta fija la escala, la relativa ajusta el sensor 4 contra
//  el INA219 de referencia.
// ════════════════════════════════════════════════════════════════════════════

float applyCurrentCalib(int sensor, float current_mA) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return current_mA;

    float result = current_mA;

    // Calibración absoluta — aplica a todos los sensores
    if (tablaCorreinte[sensor].activa && tablaCorreinte[sensor].count > 0) {
        result = interpolate(tablaCorreinte[sensor], result);
    }

    // Calibración relativa — solo corrige el sensor 4, con la tabla de la
    // referencia INA219 seleccionada
    if (sensor == CALIB_SENSOR_REL &&
        sensorCargaActivo >= 0 && sensorCargaActivo < NUMBER_OF_SENSORS) {
        const CalibTabla_t& rel = tablaRelativa[sensorCargaActivo];
        if (rel.activa && rel.count > 0) {
            result = interpolate(rel, result);
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

// Atención: afecta a AMBAS tablas. Para tocar solo una usar
// setAbsoluteCalibFlag() / setRelativeCalibFlag().
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

void imprimirCalibracionRelativa(int sensorRef) {
    if (sensorRef < 0 || sensorRef >= NUMBER_OF_SENSORS-1) return;
    CalibTabla_t* t = &tablaRelativa[sensorRef];

    writeSerialComln(String("[CALIB] === Calibración relativa sensor") + String(CALIB_SENSOR_REL) +
                     String(" vs ref ") + String(sensorRef) + String(" ==="));
    writeSerialComln(String("  Activa: ") + String(t->activa ? "SI" : "NO") +
                     String(sensorCargaActivo == sensorRef ? "  (SELECCIONADA)" : ""));
    writeSerialComln(String("  Puntos: ") + String(t->count));
    for (int i = 0; i < t->count; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "  [%d] sensor%d=%.2f mA  ref=%.2f mA",
                 i, CALIB_SENSOR_REL, t->puntos[i].measured, t->puntos[i].real);
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


// ════════════════════════════════════════════════════════════════════════════
//  Selección de la referencia relativa activa
//
//  ref = INA219 cuya tabla relativa se aplica al sensor 4; -1 = ninguna.
//  Devuelve false si la referencia no es válida o no tiene tabla activa.
//  La selección se persiste en NVS.
// ════════════════════════════════════════════════════════════════════════════

bool setSensorCargaActivo(int ref) {
    if (ref != -1) {
        if (ref < 0 || ref >= NUMBER_OF_SENSORS || ref == CALIB_SENSOR_REL) {
            writeSerialComln(String("[CALIB] Referencia invalida: ") + String(ref));
            return false;
        }
        if (!(tablaRelativa[ref].activa && tablaRelativa[ref].count > 0)) {
            writeSerialComln(String("[CALIB] Sin tabla relativa activa para ref ") + String(ref));
            return false;
        }
    }
    sensorCargaActivo = ref;
    persistirRefActiva(ref);
    writeSerialComln(String("[CALIB] Referencia relativa activa: ") + String(ref));
    return true;
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


// ════════════════════════════════════════════════════════════════════════════
//  Flags independientes (absoluta / relativa)
// ════════════════════════════════════════════════════════════════════════════

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
    // Si se desactiva la referencia seleccionada, deja de aplicarse
    if (!activa && sensorCargaActivo == sensor) {
        sensorCargaActivo = -1;
    }
}

bool getRelativeCalibFlag(int sensor) {
    if (sensor < 0 || sensor >= NUMBER_OF_SENSORS) return false;
    return tablaRelativa[sensor].activa;
}