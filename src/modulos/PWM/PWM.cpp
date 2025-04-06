#include "PWM.h"


const int PWM_CHANNEL = 0;       // Canal PWM (ESP32 tiene 16 canales disponibles: 0-15)
const int PWM_FREQ = 312000;     // Frecuencia PWM deseada: 312 kHz
const int PWM_RESOLUTION = 8;    // Resolución de 8 bits (valores de duty cycle entre 0 y 255)

const int MAX_DUTY_CYCLE = (int)(pow(2, PWM_RESOLUTION) - 1); // Valor máximo del duty cycle
const int LED_OUTPUT_PIN = 18;   // Pin GPIO donde se genera la señal PWM

// Variables globales
static int DC = 0;  // Duty cycle inicial 


void PWMInit(){

  // Configuración del canal PWM con frecuencia y resolución
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  // Asociar el canal PWM al pin de salida
  ledcAttachPin(LED_OUTPUT_PIN, PWM_CHANNEL);



}

void PWMUpdate(){


  // Aplicar el duty cycle actual
  int pwmValue = (DC * MAX_DUTY_CYCLE) / 100; // Escalar a 0-255
  ledcWrite(PWM_CHANNEL, pwmValue);

}

bool PWMSetDC(int dc){
  if(dc>=0 && dc<=100){
    DC = dc;
    return true;

  }
  return false;
}