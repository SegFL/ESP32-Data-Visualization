

#include "pid.h"
#define MAX_SAFE_CURRENT 400.0f


// ====== Parámetros PID ======
float Kp = 0.05f;
float Ki = 0.01f;
float Kd = 0.0f;
//período de muestreo real (200ms) Tiene que coincidir con el periodo del task2 
//que se encarga de leer el ADC y actualizar la carga electronica
float Ts = 0.2f;
float integral = 0.0f;
float error_anterior = 0.0f;

// ====== DutyCycle actual (0..100%) ======
float duty_percent = 0.0f;

// ====== Función de control ======
float getDCPID(float referencia_mA) {
    // 1. Leer corriente medida
    float I_meas = getLastCurrentData();  // en mA

    // 2. Calcular error
    float error = referencia_mA - I_meas;

    // 3. Integración
    integral += error * Ts;

    // 4. Derivación (derivada del error)
    float derivada_error = (error - error_anterior) / Ts;
    
    // 5. Acción de control PID (incremental en duty)
    float u = Kp * error + Ki * integral + Kd * derivada_error;

    duty_percent += u;

    // 6. Saturar duty (0..100%) y anti-windup
    if (duty_percent > 100.0f) {
        duty_percent = 100.0f;
        integral -= error * Ts; // anti-windup
    }
    if (duty_percent < 0.0f) {
        duty_percent = 0.0f;
        integral -= error * Ts;
    }

    // 7. Guardar error para próxima iteración
    error_anterior = error;

    // 8. Devolver duty en rango [0..100%]
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

