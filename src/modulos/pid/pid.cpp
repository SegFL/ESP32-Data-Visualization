/*
#define MAX_SAFE_CURRENT 400f

// ---------- parámetros PID ----------
float Kp = 0.3f;
float Ti = 10.0f;   // segundos
float Td = 0.0f;    // segundos
float Ts = 0.2f;    // segundos (muestreo cada 200 ms)

float I_max = 0.8f;  // límites integrador (en unidades de salida)
float I_min = -0.8f;

float out_min = 0.0f;   // por ejemplo duty 0.0 .. 1.0
float out_max = 1.0f;

float integrator = 0.0f;
float measured_prev = 0.0f;

// setpoint y mediciones
volatile float setpoint_current = 0.0f; // en Amperios
volatile float measured_current = 0.0f; // actualizar desde I2C read

// ---------- función de control, llamada cada Ts (ej: por timer o bucle con sleep) ----------
void pid_control_step(void) {
    float error = setpoint_current - measured_current;

    // P
    float P = Kp * error;

    // I (usar incremento proporcional a Kp/Ti)
    if (Ti > 0.0f) {
        float I_increment = Kp * (Ts / Ti) * error;
        integrator += I_increment;
        if (integrator > I_max) integrator = I_max;
        if (integrator < I_min) integrator = I_min;
    } else {
        integrator = 0.0f;
    }

    // D (sobre medicion para evitar kick)
    float D = 0.0f;

    if (Td > 0.0f) {
        D = -Kp * (Td / Ts) * (measured_current - measured_prev);
    }
   

    // salida no saturada
    float out_unsat = P + integrator + D;

    // saturar salida
    float out = out_unsat;
    if (out > out_max) out = out_max;
    if (out < out_min) out = out_min;

    // Anti-windup simple (no integrar si saturado y error empuja más hacia saturación)
    if ( (out == out_max && error > 0.0f) || (out == out_min && error < 0.0f) ) {
        // revertir último incremento de integrador
        if (Ti > 0.0f) integrator -= Kp * (Ts / Ti) * error;
        // alternativa: usar back-calculation (más fino)
    }

    // aplicar salida al actuador
    // ejemplo: actuador espera 0..1 float -> mapear a PWM o DAC

    // protección rápida
    if (measured_current > MAX_SAFE_CURRENT)
        actuador_set(0.0f); // apagar

    actuador_set(out);

    // actualizar prev
    measured_prev = measured_current;

    // protección rápida
    if (measured_current > MAX_SAFE_CURRENT) emergency_shutdown();
}
*/