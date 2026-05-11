

#include "pid.h"
#define MAX_SAFE_CURRENT 2000.0f


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


float Kp = 1.0f;
float Ki = 1.0f;
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

    p->duty = u;

    // saturación + anti-windup simple
    if (p->duty > 100.0f) {
        p->duty = 100.0f;
    } else if (p->duty < 0.0f) {
        p->duty = 0.0f;
    }

    p->error_prev = error;


    char buf[128];

    /*
    if(index==0){
                float term_p = p->Kp * error;
        float term_i = p->Ki * p->integral;
        float term_d = p->Kd * derivative;

        snprintf(buf, sizeof(buf),
            "[PID %d] ref=%.2f meas=%.2f err=%.3f",
            index, ref_percent, meas_percent, error);
        writeSerialComln(buf);

        snprintf(buf, sizeof(buf),
            "[PID %d]  P=%.3f  I=%.3f  D=%.3f  u=%.3f  duty=%.2f",
            index, term_p, term_i, term_d, u, p->duty);
        writeSerialComln(buf);
    }

*/
    return p->duty;
}


// Función para configurar parámetros PID (Kp, Ki y Kd)
void setPIDParams(int index, float kp, float ki, float kd) {
    if (index < 0 || index >= MAX_CURVES) return;
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
    if (index < 0 || index >= MAX_CURVES) return false;
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
    if (index < 0 || index >= MAX_CURVES) return;
    if (kp != NULL) *kp = pid[index].Kp;
    if (ki != NULL) *ki = pid[index].Ki;
    if (kd != NULL) *kd = pid[index].Kd;
}

// Función para leer el período de muestreo Ts
float getPIDTs(int index) {
    if (index < 0 || index >= MAX_CURVES) return 0.0f;
    return pid[index].Ts;
}

