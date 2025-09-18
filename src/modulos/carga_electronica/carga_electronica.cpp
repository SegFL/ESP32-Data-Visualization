#include "carga_electronica.h"
//#define PRUEBA_CURVAS 0 //Si se define como 1 se habilita la prueba de curvas, si no se deja como 0


#include <modulos/simuladorCurvas/simuladorCurvas.h>
// CORRECCIÓN: Eliminada variable global curve innecesaria que causaba confusión 

const int PWM_CHANNEL = 0;       // Canal PWM (ESP32 tiene 16 canales disponibles: 0-15)
const int PWM_FREQ = 312000;     // Frecuencia PWM deseada: 312 kHz
const int PWM_RESOLUTION = 8;    // Resolución de 8 bits (valores de duty cycle entre 0 y 255)
//La resolucion maxima depende de la frecuecnia utilizada, si se quiere mas frecuencia se tiene que
//sacrificar resolucion



const int MAX_DUTY_CYCLE = (int)(pow(2, PWM_RESOLUTION) - 1); // Valor máximo del duty cycle
const int LED_OUTPUT_PIN = 18;   // Pin GPIO donde se genera la señal PWM

// Variables globales
int DC = 0;  // Duty cycle actual (0-100%)

int max_dc_value = 0; // Valor máximo del duty cycle (0-100%)

int valorSensado = 0; // Valor sensado de la corriente (mA) por el INA219
modoFuncionamiento_t modoFuncionamiento = NONE; // Modo de funcionamiento inicial (PID o directo/NONE)
//referenceMode_t referenceMode = interface_state; // Modo de referencia inicial (interfaz o curva)

curve_mode_t curveMode = ON_t; // Decide si le hace caso a los datos de la curva o a los del usuario

bool arraySelected=false;
int arraySelectedPos=-1;



int getDCPID(int dc);

void CargaElectronicaInit(){

  // Configuración del canal PWM con frecuencia y resolución
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  // Asociar el canal PWM al pin de salida
  ledcAttachPin(LED_OUTPUT_PIN, PWM_CHANNEL);
  DC=0; // Inicializar el duty cycle a 0
  max_dc_value=0; // Inicializar el valor máximo del duty cycle a 0
  ledcWrite(PWM_CHANNEL, 0); // Inicializar el PWM a 0 (apagado)

  simuladorCurvasInit(3);

        //Crear la curva usando la nueva función encapsulada
        int curveId = createCurve(0);
        if(curveId == -1){
            writeSerialComln(String("Error al crear la curva"));
            return;
        }
        
        //Agregar puntos a la curva usando la nueva función encapsulada
        addPointToCurve(curveId, 30, 30);
        addPointToCurve(curveId, 40, 40);
        addPointToCurve(curveId, 50, 50);
        addPointToCurve(curveId, 60, 30);
        addPointToCurve(curveId, 70, 50);
        addPointToCurve(curveId, 80, 0);
        
        writeSerialComln(String("Curva creada con ID: ") + String(curveId));
        // Las funciones sendCurves y printCurves ahora usan el array interno
        sendCurves();
        printCurves();
 
        

}
void CargaElectronicaUpdate(){
  // CORRECCIÓN CRÍTICA: Función ultra-simplificada para evitar stack overflow
  // Eliminamos TODOS los logs para reducir el uso de stack
  int dutyCycleAux = 0;
  int referencia = 0;
  int aux = 0;

  // Selección de referencia
  switch(curveMode){
    case OFF_t:
      referencia = DC; // Usar siempre valor manual
      break;

    case ON_t:        
      aux = getCurveValue(0);
      // ⚠️ Ojo: este log puede consumir stack, comentar si hay problemas
      writeSerialComln(String("Valor de la curva: ") + String(aux));
      writeSerialComln(String("Valor de la curva: ") + String(aux));

      if(aux != -1){
        referencia = aux; // Usar valor de la curva si es válido
      } else {
        // Si no hay valor válido, salir sin cambiar nada
        return;
      }
      break;

    default:
      referencia = 0;
      break;
  }

  // Selección de modo de funcionamiento
  switch(modoFuncionamiento){
    case PID: 
      dutyCycleAux = getDCPID(referencia);
      break;

    case NONE: 
      dutyCycleAux = referencia;
      break;

    default:
      dutyCycleAux = 0;
      break;
  }
  
  // Aplicar el duty cycle actual (invertido)
  int pwmValue = (int)((100.0 - dutyCycleAux) * MAX_DUTY_CYCLE / 100.0);
  ledcWrite(PWM_CHANNEL, pwmValue);
}


//Cambia elvalor del duty cycle
// Se espera un valor entre 0 y 100, si el valor es mayor al máximo permitido se limita al máximo permitido
float PWMSetDC(float dc) {
  if (dc >= 0.0 && dc <= 100.0) {
    if (dc <= max_dc_value) {
      DC = dc; 
    } else {
      DC = max_dc_value; 
    }
    return DC;
  }
  return -1.0; // Valor inválido
}


void PWMSetCurveMode(curve_mode_t state){
  curveMode = state;
}

void printCargaElectronica(){
    // Ahora usa el array interno del módulo simuladorCurvas
    printCurves();
}


bool PWMSetFrequency(int frecuencies){
  if(frecuencies>0 && frecuencies<500000){
    ledcSetup(PWM_CHANNEL, frecuencies, PWM_RESOLUTION);
    return true;
  }
  return false;

}
bool PWMSetMaxDC(float dc){
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




