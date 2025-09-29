

#include "pid.h"
#define MAX_SAFE_CURRENT 400.0f


// ====== Parámetros PI ======
float Kp = 0.05f;
float Ki = 0.01f;
float Ts = 0.1f;   // período de muestreo (segundos)
float integral = 0.0f;

// ====== DutyCycle actual (normalizado 0..1) ======
float duty_norm = 0.0f;

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

    duty_norm += u;

    // 5. Saturar duty (0..1) y anti-windup
    if (duty_norm > 1.0f) {
        duty_norm = 1.0f;
        integral -= error * Ts; // anti-windup
    }
    if (duty_norm < 0.0f) {
        duty_norm = 0.0f;
        integral -= error * Ts;
    }

    // 6. Devolver duty en rango [0..1]
    return duty_norm;
}

