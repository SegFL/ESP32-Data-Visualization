

#include "pid.h"
#include <modulos/carga_electronica/carga_electronica.h>

#define MAX_DUTY 95.0F
#define MIN_DUTY 0.0f

#define MAX_SAFE_CURRENT 2000.0f

#define LUT_MAX_POINTS 25

// ====== Parámetros PID ======
//float Kp = 0.1f;
//float Ki = 0.1f;
//float Kd = 0.0f;


typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float Ts;

    float integral;
    float error_prev;
    float duty;
    bool  use_feedforward;  

} PID_t;

typedef enum {
    CURRENT,
    VOLTAGE,
    POWER
} PID_mode_t;

static PID_mode_t PID_MODE[NUMBER_OF_SENSORS] = {CURRENT, CURRENT, CURRENT}; // Modo de control para cada sensor (corriente o voltaje)



static PID_t pid[NUMBER_OF_SENSORS];


float Kp = 1.0f;
float Ki = 1.0f;
float Kd = 0.0f;

//período de muestreo real (200ms) Tiene que coincidir con el periodo del task2 
//que se encarga de leer el ADC y actualizar la carga electronica
float Ts = 0.1f;
float integral = 0.0f;
float error_anterior = 0.0f;

// ====== DutyCycle actual (0..100%) ======
float duty_percent = 0.0f;

static void cargarLUTFeedforward(int index);

void PID_Init(int index, float kp, float ki, float kd, float ts) {
    if (index < 0 || index >= NUMBER_OF_SENSORS) return;

    pid[index].Kp = kp;
    pid[index].Ki = ki;
    pid[index].Kd = kd;
    pid[index].Ts = ts;

    pid[index].integral = 0.0f;
    pid[index].error_prev = 0.0f;
    pid[index].duty = 0.0f;
    pid[index].use_feedforward = false;
    cargarLUTFeedforward(index);

}

// ====== Función de control ======
float getDCPID(float setPoint, int index) {
    static float meas_filtered[NUMBER_OF_SENSORS] = {0.0f};


    //writeSerialComln(String("Ejecutando PID para referencia ") + String(setPoint) + String(" mA en index ") + String(index));
    if (index < 0 || index >= NUMBER_OF_SENSORS) return 0.0f;

    PID_t *p = &pid[index];

    float denom;
    float ref_percent;
    float meas_percent;
    float meas;

    switch(PID_MODE[index]){
        case VOLTAGE: {
            #define MAX_SAFE_VOLTAGE 30.0f  // ajustá según tu hardware
            denom       = MAX_SAFE_VOLTAGE;
            ref_percent = (setPoint * 100.0f) / denom;  // acá "setPoint" es en realidad voltios
            meas  = getLastBusVoltage(index);

            break;
        }
        case CURRENT:{
            denom       = MAX_SAFE_CURRENT;
            ref_percent = (setPoint * 100.0f) / denom;
            meas  = getLastCurrentData(index);

            break;
        }
        case POWER:{
            #define MAX_SAFE_POWER 60000.0f // ajustá según tu hardware (ej: 60W = 12V * 5A)
            denom       = MAX_SAFE_POWER;
            ref_percent = (setPoint * 100.0f) / denom;  // acá "setPoint" es en realidad potencia en mW
            meas = getLastPowerData(index);

            break;            
        }
        default: {
            denom       = MAX_SAFE_CURRENT;
            ref_percent = (setPoint * 100.0f) / denom;
            meas  = getLastCurrentData(index);

            break;
        }
    }

    //Aplico un filtro a los valores medidos. Por ahora voy a hacer un simple promedio 
    //Esto es un movieng average exponencial con alpha=0.5, que es un buen compromiso entre suavizado y respuesta rápida

    meas_filtered[index] = 0.2f * meas + 0.8f * meas_filtered[index];
    meas_percent = (meas_filtered[index] * 100.0f) / denom;



    float error = ref_percent - meas_percent;

    // En modo voltaje la planta tiene ganancia negativa: más duty = menos tensión
    if (PID_MODE[index] == VOLTAGE) {
        error = -error;
    }

    if (setPoint < 0.0f) {
        p->integral   = 0.0f;
        p->error_prev = 0.0f;
        p->duty       = MIN_DUTY;
        return MIN_DUTY;  // 10% = 0 mA, apagado funcional
    }

    // ── Ciclo de STEP: aplicar feedforward y salir ──
    if (p->use_feedforward) {
        p->use_feedforward = false;
        p->duty = feedforward(setPoint, index);
        if (p->duty > MAX_DUTY) p->duty = MAX_DUTY;
        else if (p->duty < MIN_DUTY) p->duty = MIN_DUTY;

        // Inicializar integral en el punto de operación del ff, con límite
        float integral_max = (p->Ki > 0.0f) ? (100.0f / p->Ki) : 1000.0f;
        p->integral = p->duty / p->Ki;
        if (p->integral >  integral_max) p->integral =  integral_max;
        if (p->integral < -integral_max) p->integral = -integral_max;

        p->error_prev = 0.0f;
        return p->duty;
    }

    // ── Ciclos normales: PID completo ──
    p->integral += error * p->Ts;

    // Anti-windup: límite absoluto del integrador
    float integral_max = (p->Ki > 0.0f) ? (100.0f / p->Ki) : 1000.0f;
    if (p->integral >  integral_max) p->integral =  integral_max;
    if (p->integral < -integral_max) p->integral = -integral_max;

    float derivative = (error - p->error_prev) / p->Ts;
    float u = p->Kp * error + p->Ki * p->integral + p->Kd * derivative;

    // Anti-windup: clamping — si saturado, deshacer integración
    bool saturado_alto = (u > MAX_DUTY) && (error > 0.0f);
    bool saturado_bajo = (u < MIN_DUTY)   && (error < 0.0f);
    if (saturado_alto || saturado_bajo) {
        p->integral -= error * p->Ts;
    }

    p->duty = u;
    if (p->duty >MAX_DUTY) p->duty = MAX_DUTY;
    else if (p->duty < MIN_DUTY) p->duty = MIN_DUTY;

    p->error_prev = error;

    if(index == 1) writeSerialComln(String("PID: ref=") + String(setPoint) + String(" mA, meas=") + String(meas) + String(", duty=") + String(p->duty));
    
    return p->duty;
}


// Función para configurar parámetros PID (Kp, Ki y Kd)
void setPIDParams(int index, float kp, float ki, float kd) {
    if (index < 0 || index >= NUMBER_OF_SENSORS ) return;
    pid[index].Kp = kp;
    pid[index].Ki = ki;
    pid[index].Kd = kd;
}


// Función para configurar el período de muestreo Ts
void setPIDTs(float ts) {
    if(ts <= 0.1f) 
        Ts=0.2f; // Valor mínimo razonable
    else{   
        Ts = ts;
    }

}

// Función para resetear el controlador PID
bool resetPID(int index) {
    if (index < 0 || index >= NUMBER_OF_SENSORS) return false;
    // Establecer parámetros PID transparentes (sin acción de control)
    pid[index].Kp = 1.0f;
    pid[index].Ki = 1.0f;
    pid[index].Kd = 0.0f;
    pid[index].Ts = 0.2f;  // Período real de muestreo (200ms)
    
    // Resetear variables internas
    pid[index].integral = 0.0f;
    pid[index].error_prev = 0.0f;
    pid[index].duty = 0.0f;
    return true;
}

// Función para leer los parámetros PID actuales (Kp, Ki y Kd)
void getPIDParams(int index, float* kp, float* ki, float* kd) {
    if (index < 0 || index >= NUMBER_OF_SENSORS) return;
    if (kp != NULL) *kp = pid[index].Kp;
    if (ki != NULL) *ki = pid[index].Ki;
    if (kd != NULL) *kd = pid[index].Kd;
}

// Función para leer el período de muestreo Ts
float getPIDTs(int index) {
    if (index < 0 || index >= NUMBER_OF_SENSORS) return 0.0f;
    return pid[index].Ts;
}



#define LUT_MAX_CHANNELS 5


typedef struct {
    float current_mA;
    float duty_percent;
} LUT_Point_t;

LUT_Point_t LUT[LUT_MAX_CHANNELS][LUT_MAX_POINTS] = {
    // Canal 0: sin calibrar
    { {-1, -1} },

    // Canal 1: calibrado
    {   {  0.0f,  45.0f },
        {   7.6f,  50.0f },
        { 160.0f,  60.0f },
        { 295.0f,  65.0f },
        { 443.0f,  70.0f },
        { 590.0f,  75.0f },
        { 760.0f,  80.0f },
        { 920.0f,  85.0f },
        {1100.0f,  90.0f },
        {1400.0f,  95.0f },
        {1800.0f, 100.0f },
    },

    // Canal 2, 3, 4: sin calibrar
    { {-1, -1} },
    { {10.0f, 15.0f} },
    { {-1, -1} },
};

float feedforward(float referencia_mA, int index) {
    if (index < 0 || index >= LUT_MAX_CHANNELS) return 0.0f;

    // Buscar el punto más cercano por corriente
    float best_duty = 0.0f;
    float best_dist = 1e9f;

    for (int i = 0; i < LUT_MAX_POINTS; i++) {
        if (LUT[index][i].current_mA < 0) continue; // fin de datos

        float dist = fabsf(LUT[index][i].current_mA - referencia_mA);
        if (dist < best_dist) {
            best_dist = dist;
            best_duty = LUT[index][i].duty_percent;
        }
    }


    return best_duty;
}


void PID_EnableFeedforward(int index) {
    if (index < 0 || index >= NUMBER_OF_SENSORS) return;
    pid[index].use_feedforward = true;
    pid[index].error_prev      = 0.0f;
}


char getPIDMode(int index){
    if (index < 0 || index >= NUMBER_OF_SENSORS) return CURRENT;
    char mode;

    switch(PID_MODE[index]){
        case CURRENT:
            mode = 'i';
            break;
        case VOLTAGE:
            mode = 'v';
            break;
        case POWER:
            mode = 'p';
            break;
        default:
            mode = 'i';
            break;
    }
    return mode;
}

bool setPIDMode(int index, char mode){
    if (index < 0 || index >= NUMBER_OF_SENSORS) return false;

    switch(mode){
        case 'i':
            PID_MODE[index] = CURRENT;
            return true;
        case 'v':
            PID_MODE[index] = VOLTAGE;
            return true;
        case 'p':
            PID_MODE[index] = POWER;
            return true;
        default:
            return false;  
    }

        return false;
}


static void cargarLUTFeedforward(int index) {
    int numPuntos = 0;
    PuntoIdentificado_t* datos = getIdentificacionNVS(index, &numPuntos);
    if (datos == nullptr) {
        writeSerialComln(String("LUT canal ") + String(index) + String(": sin identificacion guardada"));
        return;
    }

    int maxPuntos = min(numPuntos, LUT_MAX_POINTS);
    for (int i = 0; i < maxPuntos; i++) {
        LUT[index][i].current_mA   = datos[i].corriente_mA;
        LUT[index][i].duty_percent = datos[i].duty_percent;
        writeSerialComln(String("  LUT[") + String(index) + String("][") + String(i) + String("]: ") 
            + String(datos[i].corriente_mA) + String(" mA -> ") + String(datos[i].duty_percent) + String("%"));
    }
    if (maxPuntos < LUT_MAX_POINTS) {
        LUT[index][maxPuntos].current_mA   = -1;
        LUT[index][maxPuntos].duty_percent = -1;
    }

    free(datos);
    writeSerialComln(String("LUT canal ") + String(index) + String(": ") + String(maxPuntos) + String(" puntos cargados"));
}