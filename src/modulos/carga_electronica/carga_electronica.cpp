


#include "carga_electronica.h"
//#define PRUEBA_CURVAS 0 //Si se define como 1 se habilita la prueba de curvas, si no se deja como 0
#include "driver/ledc.h"

#include <modulos/simuladorCurvas/simuladorCurvas.h>

float convertirCorrienteADc(float reference_current);
void cargarConfiguracionNvs();
float getDCDirecto(float ref);
static int calcularMaxDuty(int resolution);
#define MAX_DC_NVS_KEY  "max_dc_value"
#define MODO_FUNCIONAMIENTO_NVS_KEY  "MODO_FUNC"


typedef struct {
    int channel;
    int timer;        
    int freq;
    int resolution;
    int pin;
    int max_duty;  
} PWM_Config_t;
/*
Pin 6 ESP32 = PIN0 TERMINAL = EL0 = GPIO 16 (TIMER 0)
Pin 7 ESP32 = PIN1 TERMINAL = EL1 = GPIO 17 (TIMER 1)
Pin 24 ESP32 = PIN2  TERMINAL = EL2_HV = GPIO 26 (TIMER 2)
Pin 25 ESP32 = PIN3  TERMINAL = EL3_HV = GPIO 27 (TIMER 2) (comparte timer con GPIO 26)
Pin 22 ESP32 = PIN4  TERMINAL = AUXILIAR = GPIO 33 (TIMER 3)
*/

PWM_Config_t pwmConfig[NUMBER_OF_ELECTRONIC_LOADS] = {
    { .channel = 0, .timer = 0, .freq = 78125, .resolution = 10, .pin = 16, .max_duty = 0},  // GPIO 16 → Timer 0
    { .channel = 1, .timer = 1, .freq = 78125, .resolution = 10, .pin = 17, .max_duty = 0},  // GPIO 17 → Timer 1
    { .channel = 2, .timer = 2, .freq = 78125, .resolution = 10, .pin = 26, .max_duty = 0},  // GPIO 26 → Timer 2
    { .channel = 3, .timer = 2, .freq = 78125, .resolution = 10, .pin = 27, .max_duty = 0},  // GPIO 27 → Timer 2 (comparte timer con GPIO 26)
    { .channel = 4, .timer = 3, .freq = 78125, .resolution = 10, .pin = 33, .max_duty = 0}   // GPIO 33 → Timer 3
};


//La resolucion maxima depende de la frecuecnia utilizada, si se quiere mas frecuencia se tiene que
//sacrificar resolucion




// Variables globales
float DC = 0;  // Duty cycle actual (0-100%)

float max_dc_value[NUMBER_OF_ELECTRONIC_LOADS]={100.0f}; // Valor máximo del duty cycle (0-100%)
float maxCurrent=1000.0f; // Valor máximo de corriente en mA
int valorSensado = 0; // Valor sensado de la corriente (mA) por el INA219
float currentReference_mA[NUMBER_OF_ELECTRONIC_LOADS] = {0.0f};

modoFuncionamiento_t modoFuncionamiento[NUMBER_OF_ELECTRONIC_LOADS]={NONE} ; // Modo de funcionamiento inicial (PID o directo/NONE)
//referenceMode_t referenceMode = interface_state; // Modo de referencia inicial (interfaz o curva)

curve_mode_t curveMode[NUMBER_OF_ELECTRONIC_LOADS] ={OFF_t}; // Decide si le hace caso a los datos de la curva o a los del usuario

bool arraySelected=false;
int arraySelectedPos=-1;


static void makeKey(char *out, const char *base, int index) ;

void CargaElectronicaInit(){

  // Configuración del canal PWM con frecuencia y resolución
  //ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
/*
  for(int i=0;i<NUMBER_OF_ELECTRONIC_LOADS;i++){
    ledcSetup(
        pwmConfig[i].channel,
        pwmConfig[i].freq,
        pwmConfig[i].resolution
    );
  }

  // Asociar el canal PWM al pin de salida
  ledcAttachPin(pwmConfig[0].pin, pwmConfig[0].channel);
  ledcAttachPin(pwmConfig[1].pin, pwmConfig[1].channel);
*/

    for(int i = 0; i < NUMBER_OF_ELECTRONIC_LOADS; i++) {  
        // Calcular max_duty ANTES de configurar
        pwmConfig[i].max_duty = calcularMaxDuty(pwmConfig[i].resolution);

        // CONFIGURAR TIMER
        ledc_timer_config_t timer = {};
        timer.speed_mode = LEDC_HIGH_SPEED_MODE;
        timer.timer_num = (ledc_timer_t)pwmConfig[i].timer;
        timer.freq_hz = (uint32_t)pwmConfig[i].freq;
        timer.duty_resolution = (ledc_timer_bit_t)pwmConfig[i].resolution;
        timer.clk_cfg = LEDC_AUTO_CLK;

        ledc_timer_config(&timer);

        // CONFIGURAR CANAL
        ledc_channel_config_t channel = {};
        channel.gpio_num = pwmConfig[i].pin;
        channel.channel = (ledc_channel_t)pwmConfig[i].channel;
        channel.timer_sel = (ledc_timer_t)pwmConfig[i].timer;
        channel.duty = 0;
        channel.speed_mode = LEDC_HIGH_SPEED_MODE;

        ledc_channel_config(&channel);
    }

  DC = 0.0f; // Inicializar el duty cycle a 0 (%)
  for(int i=0;i<NUMBER_OF_ELECTRONIC_LOADS;i++){
    max_dc_value[i] = 100.0f; // Inicializar el valor máximo del duty cycle a 100% (sin recorte)
    ledcWrite(pwmConfig[i].channel,0); // Inicializar el PWM a 0 (apagado)

    setCurrentReference_mA(0.0f,i); // referencia manual en mA
    writeSerialComln(String("Reference set to : ") + String(getCurrentReference_mA(i)) + String(" mA for channel ") + String(i));

  }
  
  simuladorCurvasInit(4);//Son 4 curvas porque la HV son 2 en una

        //Crear la curva usando la nueva función encapsulada
        /*
        int curveId = createCurve(1);
        if(curveId == -1){
            writeSerialComln(String("Error al crear la curva"));
            return;
        }
*/

        //Agregar puntos a la curva usando la nueva función encapsulada
        //addPointToCurve(curveId, 10, 30.0f,STEP);

        
        // Las funciones sendCurves y printCurves ahora usan el array interno
        sendCurves();
        printCurves();
 
        for(int i=0; i<NUMBER_OF_ELECTRONIC_LOADS; i++){
            currentReference_mA[i] = 0.0f;
            max_dc_value[i] = 100.0f;
            ledcWrite(pwmConfig[i].channel, 0);
            
            // ⚠️ NO USAR setCurrentReference_mA aquí, asignación directa
            writeSerialComln(String("Canal ") + String(i) + 
                            String(" inicializado: ") + String(currentReference_mA[i]) + String(" mA"));
        }

        cargarConfiguracionNvs();


        //Inicilizo los pid
        PID_Init(0,0.020,0.050,0.000,0.1);
        PID_Init(1,0.020,0.050,0.000,0.1);



    
}
void cargarConfiguracionNvs() {

    char key[32];

    for (int i = 0; i < NUMBER_OF_ELECTRONIC_LOADS; i++) {

        /* ===== MODO DE FUNCIONAMIENTO ===== */
        makeKey(key, MODO_FUNCIONAMIENTO_NVS_KEY, i);
        char mode;
        bool error = readValueNVS(key,&mode);// si falla devuelve false y se iniciliza en NONE
        if(error==true){
          if((modoFuncionamiento_t)mode == PID){
            modoFuncionamiento[i] = PID;
          } else {
            modoFuncionamiento[i] = NONE;
          }
        } else {
          modoFuncionamiento[i] = NONE;
        }

        /* ===== MAX DC ===== */
        makeKey(key, MAX_DC_NVS_KEY, i);
        int value = 0;
        if(readValueNVSint32_t(key,&value)==0){
            writeSerialComln(String("Error al leer el valor max dc de NVS para el canal ") + String(i));
            value = -1;
        }

        if (value != -1) {
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            max_dc_value[i] = (float)value;
        } else {
            max_dc_value[i] = 100.0f;
        }

        /* ===== CURVE MODE ===== */
        char aux;
        makeKey(key, MODO_CURVA, i);
        error = readValueNVS(key, &aux);
        if(error==true){
          if((curve_mode_t)aux == ON_t){
            curveMode[i] = ON_t;
          } else {
            curveMode[i] = OFF_t;
          }
        } else {
          curveMode[i] = OFF_t;
        }
    }
}

void CargaElectronicaUpdate(){



  for(int i=0;i<NUMBER_OF_ELECTRONIC_LOADS;i++){

    float dutyCycleAux = 0.0f;
    float referenceCurrent = 0.0f; // mA
    float aux = 0.0f;
    char parameter = '0';
    
    // Selección de referencia
    switch(curveMode[i]){
      // Usar siempre valor manual
      case OFF_t:
        // En modo OFF la referencia de corriente viene de la referencia manual (mA)
        referenceCurrent = getCurrentReference_mA(i);

        break;
      case ON_t:        
        //Primero me fijo si se supero algun valor maximo o minimo de V-I-P. Esto solo lo hago en modo CURVA, porque en modo MANUAL se supone que el usuario sabe lo que hace y no le hace caso a la curva

        if(checkLimits(i,getLastCurrentData(i)/1000,0,0)){ // Por ahora solo chequeo el limite de corriente, pero se pueden agregar los de tension y potencia facilmente
          
        }
          aux = getCurveValue(i);
        //writeSerialComln("GetCurveValue :" +String(aux));
        if(aux != -1){
          //writeSerialComln("Get Curve Value: " + String(aux) + " mA");
          referenceCurrent = aux; // Usar valor de la curva si es válido
        } else {
          referenceCurrent = 0.0f;
        } break;
      default:
        referenceCurrent = 0.0f;
        break;
    }

    // Selección de modo de funcionamiento
    switch(modoFuncionamiento[i]){
      case PID:  
      //El segundo argumento es el indice del pid que se quiere usar
        dutyCycleAux = getDCPID(referenceCurrent,i);
        //writeSerialComln(String("Modo PID - Referencia: ") + String(referenceCurrent) + 
        //                String(" -> Duty Cycle: ") + String(dutyCycleAux));
        break;
      case NONE: 

      //Todo valor que reciva de referenceCurrent termina siendo un porcentaje de Duty Cycle directo
        dutyCycleAux = getDCDirecto(referenceCurrent); // Duty cycle directo
        //writeSerialComln(String("Modo NONE - Duty Cycle directo: ") + String(dutyCycleAux));
        break;
      default:   
        dutyCycleAux = 0.0f; 
        //writeSerialComln(String("Modo desconocido - Duty Cycle: 0"));
        break;
    }


/*
    // Aplicar recorte por max_dc_value
    if (max_dc_value[i] >= 0.0f && max_dc_value[i] <= 100.0f) {
        if (dutyCycleAux > max_dc_value[i]) {
            dutyCycleAux = max_dc_value[i];
            writeSerialComln(String("DC recortado a max_dc_value: ") + String(dutyCycleAux) + " %");
        }
    }*/
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
    int pwmValue = (int)((100.0f - dutyCycleAux) * pwmConfig[i].max_duty / 100.0f);
        
    //writeSerialComln(String( "Duty[") + String(i) + String("]: ") + String(dutyCycleAux)+ String("% -> PWM Value: ") + String(pwmValue));

    //writeSerialComln(String("Aplicando Duty Cycle: ") + String(dutyCycleAux) + String("% -> PWM Value: ") + String(pwmValue));
    //ledcWrite(PWM_CHANNEL, pwmValue);



    ledcWrite(pwmConfig[i].channel,pwmValue); // Inicializar el PWM a 0 (apagado)  
  }
}


//Cambia elvalor del duty cycle
// Se espera un valor entre 0 y 100, si el valor es mayor al máximo permitido se limita al máximo permitido
// Se espera un valor entre 0 y 100 (corrige comportamiento previo)
// Ahora recorta (clamp) usando max_dc_value

float PWMSetDC(float currentRef,int index) {
    if(index<0 || index>=NUMBER_OF_ELECTRONIC_LOADS) return -1.0f;
    if (currentRef < 0.0f) return -1.0f;
    // aplicar límite máximo configurado
    float limited = currentRef;
    if (max_dc_value[index] >= 0.0f && max_dc_value[index] <= 100.0f) {
        if (limited > max_dc_value[index]) limited = max_dc_value[index];
    }
    // límite físico 0..100
    if (limited > 100.0f) limited = 100.0f;
    DC = limited;
    return DC;
}


void PWMSetCurveMode(curve_mode_t state, int index){
  if(index<0 || index>=NUMBER_OF_ELECTRONIC_LOADS) return;
  // Guardar en NVS
  char key[32];
  makeKey(key, MODO_CURVA, index);
  // saveValueNVS retorna 0 en éxito
  if(saveValueNVS(key, (char)state) == 0){
      // Éxito: actualizar variable local
      curveMode[index] = state;
  } else {
      // Error al guardar
      writeSerialComln(String("Error al guardar el modo de curva en NVS"));
  }
}

void printCargaElectronica(){
    // Ahora usa el array interno del módulo simuladorCurvas
    printCurves();
}


bool PWMSetFrequency(int frecuencies, int index){
    if(index < 0 || index >= NUMBER_OF_ELECTRONIC_LOADS) return false;

    int timer = pwmConfig[index].timer;
    
    // Calcular nueva resolución óptima según frecuencia
    // Fórmula: freq_pwm = clk_source / (2^resolution)
    // Para 80MHz: resolution_max ≈ log2(80000000 / freq)
    int nueva_resolucion = pwmConfig[index].resolution;
    
    // Ajuste automático si la frecuencia es muy alta
    if(frecuencies > 100000) {
        nueva_resolucion = 8;  // Menor resolución para alta frecuencia
    } else if(frecuencies > 50000) {
        nueva_resolucion = 10;
    } else {
        nueva_resolucion = 12; // Máxima resolución para baja frecuencia
    }

    // Actualizar timer con nueva configuración
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    timer_cfg.timer_num = (ledc_timer_t)timer;
    timer_cfg.freq_hz = frecuencies;
    timer_cfg.duty_resolution = (ledc_timer_bit_t)nueva_resolucion;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    
    if(ledc_timer_config(&timer_cfg) != ESP_OK) {
        return false;
    }

    // Actualizar todos los canales que usan ese timer
    for(int i = 0; i < NUMBER_OF_ELECTRONIC_LOADS; i++){
        if(pwmConfig[i].timer == timer){
            pwmConfig[i].freq = frecuencies;
            pwmConfig[i].resolution = nueva_resolucion;
            pwmConfig[i].max_duty = calcularMaxDuty(nueva_resolucion);  // ← RECALCULAR
        }
    }

    return true;
}
bool PWMSetMaxDC(float dc,int index){
    if (index < 0 || index >= NUMBER_OF_ELECTRONIC_LOADS) return false;
    if (dc < 0.0f || dc > 100.0f) return false;
    max_dc_value[index] = dc;

    // Guardar en NVS
    nvs_handle_t handle;
    if (nvs_open("storage", NVS_READWRITE, &handle) == ESP_OK) {
        int to_store = (int)dc;
        char key[32];
        makeKey(key, MAX_DC_NVS_KEY, index);
        nvs_set_i32(handle, key, to_store);
        nvs_commit(handle);
        nvs_close(handle);
    }

    return true;
}

bool setControlMode(modoFuncionamiento_t mode, int index){
  if(index<0 || index>=NUMBER_OF_ELECTRONIC_LOADS) return false;
  // Guardar en NVS
  char key[32];
  makeKey(key, MODO_FUNCIONAMIENTO_NVS_KEY, index);
  if(saveValueNVS(key, (char)mode) == 0){  // ← Verificar que sea 0 (éxito)
      modoFuncionamiento[index] = mode;     // Actualizar solo si fue exitoso
      return true;
  }
  return false;  // Error al guardar
}

float PWMGetMaxDC(int index){
    if(index < 0 || index >= NUMBER_OF_ELECTRONIC_LOADS) return -1;
    return pwmConfig[index].max_duty;
}


// Convertir referencia de corriente (mA) a duty %
// - reference_current: mA
// - usa maxCurrent para normalizar (mA -> 0..100%)
// - respeta max_dc_value (tope %) y clampa 0..100
/*
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
*/




modoFuncionamiento_t getModoFuncionamiento(int index){
    if(index<0 || index>=NUMBER_OF_ELECTRONIC_LOADS) return NONE;
    return modoFuncionamiento[index];
}


// Nuevo: setter para referencia manual de corriente (mA) usada por PID y curva en modo OFF
bool setCurrentReference_mA(float current_mA, int index){
    if(index < 0 || index >= NUMBER_OF_ELECTRONIC_LOADS) {
        writeSerialComln(String("ERROR: Index fuera de rango: ") + String(index));
        return false;
    }
    if(current_mA < 0.0f) {
        writeSerialComln(String("ERROR: Corriente negativa: ") + String(current_mA));
        return false;
    }
    
    
    currentReference_mA[index] = current_mA;
    return true;
}


//Mapea la referencia de corriente directa al duty cycle
float getDCDirecto(float ref){
  if(ref<0.0f) return 0.0f;
  if(ref>maxCurrent) return maxCurrent;
    return 100*ref/maxCurrent; // Convertir mA a %
}


bool getPWMConfig(char index, int *channel, int *freq, int *resolution) {
    if (index < 0 || index >= NUMBER_OF_ELECTRONIC_LOADS) {
        return false;   // índice inválido
    }

    if (channel)    *channel    = pwmConfig[index].channel;
    if (freq)       *freq       = pwmConfig[index].freq;
    if (resolution) *resolution = pwmConfig[index].resolution;

    return true;
}

static void makeKey(char *out, const char *base, int index) {
    sprintf(out, "%s_%d", base, index);
}



curve_mode_t getCurveMode(int index){
    if(index<0 || index>=NUMBER_OF_ELECTRONIC_LOADS) return OFF_t;
    return curveMode[index];
}

float getCurrentReference_mA(int index){
    if(index<0 || index>=NUMBER_OF_ELECTRONIC_LOADS) return 0.0f;
    return currentReference_mA[index];
}


// Calcula el valor máximo de duty según la resolución
static int calcularMaxDuty(int resolution) {
    return (1 << resolution) - 1;  // 2^resolution - 1
    // Ejemplos:
    // resolution 10 → (1 << 10) - 1 = 1023
    // resolution 12 → (1 << 12) - 1 = 4095
    // resolution 8  → (1 << 8)  - 1 = 255
}

