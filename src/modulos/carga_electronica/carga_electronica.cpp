#include "carga_electronica.h"
//#define PRUEBA_CURVAS 0 //Si se define como 1 se habilita la prueba de curvas, si no se deja como 0


#include <modulos/simuladorCurvas/simuladorCurvas.h>
// CORRECCIÓN: Eliminada variable global curve innecesaria que causaba confusión 

float convertirCorrienteADc(float reference_current);
void cargarConfiguracionNvs();
float getDCDirecto(float ref);
const char* MAX_DC_NVS_KEY = "max_dc_value";
const char* MODO_FUNCIONAMIENTO_NVS_KEY = "MODO_FUNCIONAMIENTO";


const int PWM_CHANNEL = 0;       // Canal PWM (ESP32 tiene 16 canales disponibles: 0-15)
const int PWM_FREQ = 78125;     // Frecuencia PWM deseada: 312 kHz
const int PWM_RESOLUTION = 10;    // Resolución de 8 bits (valores de duty cycle entre 0 y 255)
//La resolucion maxima depende de la frecuecnia utilizada, si se quiere mas frecuencia se tiene que
//sacrificar resolucion



const int MAX_DUTY_CYCLE = (int)(pow(2, PWM_RESOLUTION) - 1); // Valor máximo del duty cycle
const int LED_OUTPUT_PIN = 18;   // Pin GPIO donde se genera la señal PWM

// Variables globales
float DC = 0;  // Duty cycle actual (0-100%)

float max_dc_value = 800.0; // Valor máximo del duty cycle (0-100%)
float maxCurrent=1000.0f; // Valor máximo de corriente en mA
int valorSensado = 0; // Valor sensado de la corriente (mA) por el INA219
float currentReference_mA = 0.0f;   // Referencia manual en mA usada por PID/curvas

modoFuncionamiento_t modoFuncionamiento = NONE; // Modo de funcionamiento inicial (PID o directo/NONE)
//referenceMode_t referenceMode = interface_state; // Modo de referencia inicial (interfaz o curva)

curve_mode_t curveMode = ON_t; // Decide si le hace caso a los datos de la curva o a los del usuario

bool arraySelected=false;
int arraySelectedPos=-1;




void CargaElectronicaInit(){

  // Configuración del canal PWM con frecuencia y resolución
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  // Asociar el canal PWM al pin de salida
  ledcAttachPin(LED_OUTPUT_PIN, PWM_CHANNEL);
  DC = 0.0f; // Inicializar el duty cycle a 0 (%)
  max_dc_value = 100.0f; // Inicializar el valor máximo del duty cycle a 100% (sin recorte)
  currentReference_mA = 0.0f; // referencia manual en mA
  ledcWrite(PWM_CHANNEL,0); // Inicializar el PWM a 0 (apagado)

  simuladorCurvasInit(3);

        //Crear la curva usando la nueva función encapsulada
        int curveId = createCurve(0);
        if(curveId == -1){
            writeSerialComln(String("Error al crear la curva"));
            return;
        }
        
        //Agregar puntos a la curva usando la nueva función encapsulada
        addPointToCurve(curveId, 10, 30.0f,STEP);
        addPointToCurve(curveId, 25, 40.0f,STEP);
        addPointToCurve(curveId, 50, 200.0f,STEP);
        addPointToCurve(curveId, 60, 30.0f,STEP);

        
        writeSerialComln(String("Curva creada con ID: ") + String(curveId));
        // Las funciones sendCurves y printCurves ahora usan el array interno
        sendCurves();
        printCurves();
 


        cargarConfiguracionNvs();


        //Inicilizo los pid
        PID_Init(0,0.6,0.05,0.0,0.2);



    
}


void cargarConfiguracionNvs(){

    bool mode = readValueNVS(MODO_FUNCIONAMIENTO_NVS_KEY);
    if (mode) {
        modoFuncionamiento = PID;
    } else {
        modoFuncionamiento = NONE;
    }

    int value = readValueNVSint32_t(MAX_DC_NVS_KEY);
    if(value != -1){
        // valor en % esperado 0..100
        if(value < 0) value = 0;
        if(value > 100) value = 100;
        max_dc_value = (float)value;
    } else {
        max_dc_value = 100.0f; // Valor por defecto si no se encuentra en NVS -> sin recorte
    }

    //Cargo la configuracion guardada en NVS
    if(readValueNVS(MODO_CURVA)){
        curveMode=ON_t;
    }else{
        curveMode=OFF_t;
    }




  }
void CargaElectronicaUpdate(){

  float dutyCycleAux = 0.0f;
  float referenceCurrent = 0.0f; // mA
  float aux = 0.0f;

  // Selección de referencia
  switch(curveMode){
    // Usar siempre valor manual
    case OFF_t:
      // En modo OFF la referencia de corriente viene de la referencia manual (mA)
      referenceCurrent = currentReference_mA;
      break;
    case ON_t:        
      aux = getCurveValue(0);
      if(aux != -1){
        writeSerialComln("Get Curve Value: " + String(aux) + " mA");
        referenceCurrent = aux; // Usar valor de la curva si es válido
      } else {
        referenceCurrent = 0.0f;
      } break;
    default:
      referenceCurrent = 0;
      break;
  }

  // Selección de modo de funcionamiento
  switch(modoFuncionamiento){
    case PID:  
      dutyCycleAux = getDCPID(referenceCurrent,0);
      //writeSerialComln(String("Modo PID - Referencia: ") + String(referenceCurrent) + 
       //                String(" -> Duty Cycle: ") + String(dutyCycleAux));
      break;
    case NONE: 

      dutyCycleAux = getDCDirecto(referenceCurrent); // Duty cycle directo
      //writeSerialComln(String("Modo NONE - Duty Cycle directo: ") + String(dutyCycleAux));
      break;
    default:   
      dutyCycleAux = 0.0f; 
      writeSerialComln(String("Modo desconocido - Duty Cycle: 0"));
      break;
  }
/*
  if(modoFuncionamiento==PID){
    writeSerialComln(String("Modo PID - Referencia: ") + String(referenceCurrent) + 
                     String(" -> Duty Cycle PID: ") + String(dutyCycleAux));
  }else{
    writeSerialComln(String("Modo NONE - Duty Cycle directo: ") + String(dutyCycleAux));
  }
  if(curveMode==ON_t){
    writeSerialComln(String("Modo CURVA - Referencia de corriente usada: ") + String(referenceCurrent) + String(" mA"));
  }else{
    writeSerialComln(String("Modo MANUAL - Referencia de corriente usada: ") + String(referenceCurrent) + String(" mA"));
  }
  */
  // Aplicar el duty cycle actual (invertido)
  int pwmValue = (int)((100.0f - dutyCycleAux) * MAX_DUTY_CYCLE / 100.0f);
  writeSerialComln(String("Aplicando Duty Cycle: ") + String(dutyCycleAux) + String("% -> PWM Value: ") + String(pwmValue));
  //writeSerialComln(String("Aplicando Duty Cycle: ") + String(dutyCycleAux) + String("% -> PWM Value: ") + String(pwmValue));
  ledcWrite(PWM_CHANNEL, pwmValue);
}


//Cambia elvalor del duty cycle
// Se espera un valor entre 0 y 100, si el valor es mayor al máximo permitido se limita al máximo permitido
// Se espera un valor entre 0 y 100 (corrige comportamiento previo)
// Ahora recorta (clamp) usando max_dc_value
float PWMSetDC(float currentReference) {
    if (currentReference < 0.0f) return -1.0f;
    // aplicar límite máximo configurado
    float limited = currentReference;
    if (max_dc_value >= 0.0f && max_dc_value <= 1000.0f) {
        if (limited > max_dc_value) limited = max_dc_value;
    }
    // límite físico 0..100
    if (limited > 1000.0f) limited = 1000.0f;
    DC = limited;
    return DC;
}


void PWMSetCurveMode(curve_mode_t state){
  curveMode = state;
}

void printCargaElectronica(){
    // Ahora usa el array interno del módulo simuladorCurvas
    printCurves();
}


bool PWMSetFrequency(int frecuencies){
  if(frecuencies>0 && frecuencies<PWM_FREQ){
    ledcSetup(PWM_CHANNEL, frecuencies, PWM_RESOLUTION);
    return true;
  }
  return false;

}
bool PWMSetMaxDC(float dc){
    if (dc < 0.0f || dc > 100.0f) return false;
    max_dc_value = dc;

    // Guardar en NVS
    nvs_handle_t handle;
    if (nvs_open("storage", NVS_READWRITE, &handle) == ESP_OK) {
        int to_store = (int)dc;
        nvs_set_i32(handle, MAX_DC_NVS_KEY, to_store);
        nvs_commit(handle);
        nvs_close(handle);
    }

    return true;
}

void changeControlMode(modoFuncionamiento_t mode){

  if(saveValueNVS(MODO_FUNCIONAMIENTO_NVS_KEY, mode)!=0){
      writeSerialComln(String("Error al guardar el modo de funcionamiento en NVS"));
      return;
  }


  modoFuncionamiento = mode;
}

float PWMGetMaxDC(){
    return max_dc_value;
}


// Convertir referencia de corriente (mA) a duty %
// - reference_current: mA
// - usa maxCurrent para normalizar (mA -> 0..100%)
// - respeta max_dc_value (tope %) y clampa 0..100
float convertirCorrienteADc(float reference_current){
  // Si referencia viene en % por error, proteger
  // but expected unit is mA
  if (reference_current < 0.0f) reference_current = 0.0f;

  // Evitar división por cero
  float denom = (maxCurrent > 0.0f) ? maxCurrent : 1.0f;
  float percent = (reference_current * 100.0f) / denom;

  // Clamp 0..100
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;

  // Aplicar máximo configurado en porcentaje
  if (max_dc_value >= 0.0f && max_dc_value <= 100.0f && percent > max_dc_value) {
      percent = max_dc_value;
  }

  return percent;
}

bool setMaxCurrent(float current){
    // maxCurrent está en mA. Aceptar rango razonable (0..2000 mA)
    if(current >= 0.0f && current <=2000.0f){
        maxCurrent = current;
        return true;
    }
    return false;
}



modoFuncionamiento_t getModoFuncionamiento(){
    return modoFuncionamiento;
}


// Nuevo: setter para referencia manual de corriente (mA) usada por PID y curva en modo OFF
bool setCurrentReference_mA(float current_mA){
    if (current_mA < 0.0f) return false;
    currentReference_mA = current_mA;
    return true;
}


//Mapea la referencia de corriente directa al duty cycle
float getDCDirecto(float ref){
  if(ref<0.0f) return 0.0f;
  if(ref>maxCurrent) return maxCurrent;
    return 100*ref/maxCurrent; // Convertir mA a %
}