

#include "pid.h"
#define MAX_SAFE_CURRENT 800.0f


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
} PID_t;

#define MAX_CURVES 4
static PID_t pid[MAX_CURVES];


float Kp = 0.6f;
float Ki = 0.05f;
float Kd = 0.0f;

//período de muestreo real (200ms) Tiene que coincidir con el periodo del task2 
//que se encarga de leer el ADC y actualizar la carga electronica
float Ts = 0.2f;
float integral = 0.0f;
float error_anterior = 0.0f;

// ====== DutyCycle actual (0..100%) ======
float duty_percent = 0.0f;



void PID_Init(int index, float kp, float ki, float kd, float ts) {
    if (index < 0 || index >= MAX_CURVES) return;

    pid[index].Kp = kp;
    pid[index].Ki = ki;
    pid[index].Kd = kd;
    pid[index].Ts = ts;

    pid[index].integral = 0.0f;
    pid[index].error_prev = 0.0f;
    pid[index].duty = 0.0f;
}

// ====== Función de control ======
// Reemplazar la implementación actual de getDCPID por esta
float getDCPID(float referencia_mA, int index) {
    if (index < 0 || index >= MAX_CURVES) return 0.0f;

    PID_t *p = &pid[index];

    float denom = (MAX_SAFE_CURRENT > 0.0f) ? MAX_SAFE_CURRENT : 1.0f;

    float ref_percent = (referencia_mA * 100.0f) / denom;

    float meas_mA = getLastCurrentData(index);
    float meas_percent = (meas_mA * 100.0f) / denom;

    float error = ref_percent - meas_percent;

    // integral
    p->integral += error * p->Ts;

    // derivada
    float derivative = (error - p->error_prev) / p->Ts;

    // PID
    float u = p->Kp * error + p->Ki * p->integral + p->Kd * derivative;

    p->duty += u;

    // saturación + anti-windup simple
    if (p->duty > 100.0f) {
        p->duty = 100.0f;
    } else if (p->duty < 0.0f) {
        p->duty = 0.0f;
    }

    p->error_prev = error;

    return p->duty;
}


// Función para configurar parámetros PID (Kp, Ki y Kd)
void setPIDParams(int index, float kp, float ki, float kd) {
    if (index < 0 || index >= MAX_CURVES) return;
    pid[index].Kp = kp;
    pid[index].Ki = ki;
    pid[index].Kd = kd;
}

void resetPID(int index) {
    if (index < 0 || index >= MAX_CURVES) return;

    PID_t *p = &pid[index];

    p->Kp = 0.0f;
    p->Ki = 0.0f;
    p->Kd = 0.0f;

    p->Ts = 0.2f;        // ← SIEMPRE válido

    p->integral = 0.0f;
    p->error_prev = 0.0f;
    p->duty = 0.0f;
}
// Función para configurar el período de muestreo Ts
void setPIDTs(float ts) {
    if(ts <= 0.1f) 
        setPIDTs(0.1f);
    else{   
        Ts = ts;
    }

}

// Función para resetear el controlador PID
void resetPID() {
    // Establecer parámetros PID transparentes (sin acción de control)
    Kp = 0.0f;
    Ki = 0.0f;
    Kd = 0.0f;
    Ts = 0.2f;  // Período real de muestreo (200ms)
    
    // Resetear variables internas
    integral = 0.0f;
    error_anterior = 0.0f;
    duty_percent = 0.0f;
}

// Función para leer los parámetros PID actuales (Kp, Ki y Kd)
void getPIDParams(float* kp, float* ki, float* kd) {
    if (kp != NULL) *kp = Kp;
    if (ki != NULL) *ki = Ki;
    if (kd != NULL) *kd = Kd;
}

// Función para leer el período de muestreo Ts
float getPIDTs() {
    return Ts;
}

