

#include "pid.h"
#define MAX_SAFE_CURRENT 800.0f


// ====== Parámetros PID ======
//float Kp = 0.1f;
//float Ki = 0.1f;
//float Kd = 0.0f;


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

// ====== Función de control ======
// Reemplazar la implementación actual de getDCPID por esta
float getDCPID(float referencia_mA,int index) {
    // convertir referencia y medicion a porcentaje 0..100
    float denom = (MAX_SAFE_CURRENT > 0.0f) ? MAX_SAFE_CURRENT : 1.0f;
    float ref_percent = (referencia_mA * 100.0f) / denom;

    float I_meas_mA = getLastCurrentData(index); // en mA
    float I_meas_percent = (I_meas_mA * 100.0f) / denom;

    // error en %
    float error = ref_percent - I_meas_percent;

    // integración con anti-windup en integral en unidades de %
    integral += error * Ts;

    // derivada
    float derivada_error = (error - error_anterior) / Ts;

    // acción de control (PID) en %
    float u = Kp * error + Ki * integral + Kd * derivada_error;

    duty_percent += u;

    // saturación y anti-windup más robusto: limitar integral también
    if (duty_percent > 100.0f) {
        duty_percent = 100.0f;
        // reducir integral para evitar windup
        if (Ki != 0.0f) integral = (100.0f - (Kp*error + Kd*derivada_error)) / Ki;
    } else if (duty_percent < 0.0f) {
        duty_percent = 0.0f;
        if (Ki != 0.0f) integral = (0.0f - (Kp*error + Kd*derivada_error)) / Ki;
    }

    error_anterior = error;

    // dentro de getDCPID (después de calcular duty_percent)
writeSerialComln(String("PID debug - ref%:") + String(ref_percent,2) +
                String(" meas%:") + String(I_meas_percent,2) +
                String(" err:") + String(error,2) +
                String(" u:") + String(u,3) +
                String(" int:") + String(integral,3) +
                String(" duty:") + String(duty_percent,3));


    return duty_percent;
}

// Función para configurar parámetros PID (Kp, Ki y Kd)
void setPIDParams(float kp, float ki, float kd) {
    Kp = kp;
    Ki = ki;
    Kd = kd;
    // Resetear integral y error anterior al cambiar parámetros
    integral = 0.0f;
    error_anterior = 0.0f;
}

// Función para configurar el período de muestreo Ts
void setPIDTs(float ts) {
    Ts = ts;
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

