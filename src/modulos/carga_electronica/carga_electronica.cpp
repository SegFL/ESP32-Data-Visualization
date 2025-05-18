#include "carga_electronica.h"


const int PWM_CHANNEL = 0;       // Canal PWM (ESP32 tiene 16 canales disponibles: 0-15)
const int PWM_FREQ = 312000;     // Frecuencia PWM deseada: 312 kHz
const int PWM_RESOLUTION = 8;    // Resolución de 8 bits (valores de duty cycle entre 0 y 255)

const int MAX_DUTY_CYCLE = (int)(pow(2, PWM_RESOLUTION) - 1); // Valor máximo del duty cycle
const int LED_OUTPUT_PIN = 18;   // Pin GPIO donde se genera la señal PWM

// Variables globales
int DC = 0;  // Duty cycle actual (0-100%)

int max_dc_value = 0; // Valor máximo del duty cycle (0-100%)

int valorSensado = 0; // Valor sensado de la corriente (mA) por el INA219
modoFuncionamiento_t modoFuncionamiento = NONE; // Modo de funcionamiento inicial (PID o directo/NONE)
referenceMode_t referenceMode = interface; // Modo de referencia inicial (interfaz o curva)

int getDCPID(int dc);

void CargaElectronicaInit(){

  // Configuración del canal PWM con frecuencia y resolución
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  // Asociar el canal PWM al pin de salida
  ledcAttachPin(LED_OUTPUT_PIN, PWM_CHANNEL);
  DC=0; // Inicializar el duty cycle a 0
  max_dc_value=0; // Inicializar el valor máximo del duty cycle a 0
  ledcWrite(PWM_CHANNEL, 0); // Inicializar el PWM a 0 (apagado)

}

void CargaElectronicaUpdate(){

  int dutyCycleAux=0;

  switch(referenceMode){
    case interface:{

    }
    break;
    case curve:{

    }
    break;
    default:
      break;

  }

  int referencia = DC; 

  switch(modoFuncionamiento){
    case PID: 
      dutyCycleAux = getDCPID(referencia);
      break;
    case NONE: 
      dutyCycleAux = referencia; // En modo manual, el duty cycle es el valor actual
      break;
    default:
      break;
  }


  // Aplicar el duty cycle actual
  int pwmValue = ((100-dutyCycleAux) * MAX_DUTY_CYCLE) / 100; // Escalar a 0-255 e invierto por el hardware utilizado
  ledcWrite(PWM_CHANNEL, pwmValue);

}
//Cambia elvalor del duty cycle
// Se espera un valor entre 0 y 100, si el valor es mayor al máximo permitido se limita al máximo permitido
int PWMSetDC(int dc){
  if(dc>=0 && dc<=100){
    if(dc<=max_dc_value){
      DC = dc; 
    }else{
      DC = max_dc_value; 
    }
    return DC;
  }
  return -1; // Valor inválido
}

bool PWMSetFrequency(int frecuencies){
  if(frecuencies>0 && frecuencies<500000){
    ledcSetup(PWM_CHANNEL, frecuencies, PWM_RESOLUTION);
    return true;
  }
  return false;

}

bool PWMSetMaxDC(int dc){
  if(dc>=0 && dc<=100){
    max_dc_value = dc;
    return true;
  }
  return false;
}

//Recivo el DutyCycle que busco poner a la salida y lo comparo con el valor en mA que me da
//el sensor de corriente(INA219) obteniendo un valor de DC que pongo a la salida del uC
int getDCPID(int dc){
  // Implementar la lógica del PID aquí
  // Por ahora, simplemente devolver el valor de DC
  //Si no pongo nada basicamente estoy abriendo el lazo de control
  return dc;
}

void sendToActuator(int current_mA){

//Dato recivido del sensor, se recive un valor en mA de la corriente de salida

  if(current_mA>=0){
    valorSensado= current_mA;
  }

}

