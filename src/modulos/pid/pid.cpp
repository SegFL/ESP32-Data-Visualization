

#include "pid.h"
#define MAX_SAFE_CURRENT 400.0f


// ====== Parámetros PI ======
float Kp = 0.05f;
float Ki = 0.01f;
float Ts = 0.1f;   // período de muestreo (segundos)
float integral = 0.0f;

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

    // 4. Acción de control PI (incremental en duty)
    float u = Kp * error + Ki * integral;

    duty_percent += u;

    // 5. Saturar duty (0..100%) y anti-windup
    if (duty_percent > 100.0f) {
        duty_percent = 100.0f;
        integral -= error * Ts; // anti-windup
    }
    if (duty_percent < 0.0f) {
        duty_percent = 0.0f;
        integral -= error * Ts;
    }

    // 6. Devolver duty en rango [0..100%]
    return duty_percent;
}

// Función para configurar parámetros PID
void setPIDParams(float kp, float ki, float ts) {
    Kp = kp;
    Ki = ki;
    Ts = ts;
    // Resetear integral al cambiar parámetros
    integral = 0.0f;
}

// Función para resetear el controlador PID
void resetPID() {
    integral = 0.0f;
    duty_percent = 0.0f;
}

