


#include "carga_electronica.h"
//#define PRUEBA_CURVAS 0 //Si se define como 1 se habilita la prueba de curvas, si no se deja como 0
#include "driver/ledc.h"
#include <modulos/calibration_manager/calibrationManager.h>
#include <modulos/simuladorCurvas/simuladorCurvas.h>



#define MAX_DUTY 95.0F
#define MIN_DUTY 10.0f
#define TIEMPO_STANDBY_SEGUNDOS 10
#define MUESTRAS_POR_SEGUNDO 10

float convertirCorrienteADc(float reference_current);
void cargarConfiguracionNvs();
float getDCDirecto(float ref);
static int calcularMaxDuty(int resolution);
#define MAX_DC_NVS_KEY  "max_dc_value"
#define MODO_FUNCIONAMIENTO_NVS_KEY  "MODO_FUNC"



// ============================================================
// IDENTIFICACIÓN DE PLANTA
// ============================================================
#define IDENT_DUTY_MIN              10.0f   // duty mínimo del barrido (%)
#define IDENT_DUTY_MAX              95.0f   // duty máximo del barrido (%)
#define IDENT_TIEMPO_STANDBY_S      10      // segundos en duty=0 antes de arrancar
#define IDENT_MUESTRAS_POR_SEGUNDO  10      // coincide con la frecuencia de leerADC()
#define IDENT_NVS_NAMESPACE         "ident" // namespace NVS dedicado a identificaciones



// Estado completo de una identificación en curso para un canal
typedef struct {
    float*               dutyValues;          // vector de N duties [DUTY_MIN..DUTY_MAX]
    float**              mediciones;          // mediciones[i] → vector de muestras del punto i
    PuntoIdentificado_t* resultados;          // resultados[i] → promedio procesado (se llena al final)
    int                  numPuntos;           // N puntos definidos por el usuario
    int                  muestras_por_punto;  // tiempo_por_punto_s * IDENT_MUESTRAS_POR_SEGUNDO
    int                  puntoActual;         // índice del punto que se está midiendo ahora (0..N-1)
    int                  muestraActual;       // índice dentro de mediciones[puntoActual]
    int                  llamadasTotales;     // contador global de llamadas a guardarPuntoIdentificado
    int                  curveId;             // id de la curva creada en simuladorCurvas
    bool                 activa;              // true mientras el barrido está en curso
    unsigned long        timestampInicio;     // epoch al momento de iniciar
    int                  pin;                 // canal al que pertenece esta identificación
} Identificacion_t;

// Array global de identificaciones, una por canal
static Identificacion_t* identificaciones[NUMBER_OF_SENSORS] = {nullptr};
bool identificacionEnCurso[NUMBER_OF_SENSORS] = {false};

typedef struct {
    int channel;
    int timer;        
    int freq;
    int resolution;
    int pin;
    int max_duty;  
} PWM_Config_t;

PWM_Config_t pwmConfig[NUMBER_OF_ELECTRONIC_LOADS] = {
    { .channel = 0, .timer = 0, .freq = 78125, .resolution = 10, .pin = 27, .max_duty = 0},  
    { .channel = 1, .timer = 1, .freq = 78125, .resolution = 10, .pin = 26, .max_duty = 0},  
    { .channel = 2, .timer = 2, .freq = 78125, .resolution = 10, .pin = 25, .max_duty = 0},  
    { .channel = 3, .timer = 2, .freq = 78125, .resolution = 10, .pin = 33, .max_duty = 0}, 
    { .channel = 4, .timer = 3, .freq = 78125, .resolution = 10, .pin = 32, .max_duty = 0}   
};


//La resolucion maxima depende de la frecuecnia utilizada, si se quiere mas frecuencia se tiene que
//sacrificar resolucion




// Variables globales
float DC = 0;  // Duty cycle actual (0-100%)

float max_dc_value[NUMBER_OF_ELECTRONIC_LOADS]={100.0f}; // Valor máximo del duty cycle (0-100%)
int valorSensado = 0; // Valor sensado de la corriente (mA) por el INA219
float currentReference_mA[NUMBER_OF_ELECTRONIC_LOADS] = {0.0f};

modoFuncionamiento_t modoFuncionamiento[NUMBER_OF_ELECTRONIC_LOADS]={NONE} ; // Modo de funcionamiento inicial (PID o directo/NONE)
bool feedforwardEnabled[NUMBER_OF_ELECTRONIC_LOADS] = {false}; // Indica si el feedforward está habilitado para cada canal
//referenceMode_t referenceMode = interface_state; // Modo de referencia inicial (interfaz o curva)

curve_mode_t curveMode[NUMBER_OF_ELECTRONIC_LOADS] ={OFF_t}; // Decide si le hace caso a los datos de la curva o a los del usuario

bool arraySelected=false;
int arraySelectedPos=-1;




static void makeKey(char *out, const char *base, int index) ;
static void guardarPuntoIdentificado(int pin, float corriente_mA);
static void abortarIdentificacion(int pin);
static void procesarYGuardar(int pin);
static float promediarSegundaMitad(float* muestras, int total);
static void liberarIdentificacion(int pin);
void PWMSetCurveMode(curve_mode_t state, int index);
static float calcularPromedio(float* muestras, int total);


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
        calibrationManagerInit();

        //Inicilizo los pid
        PID_Init(0,0.10,0.10,0.000,0.1);
        PID_Init(1,0.10,0.80,0.000,0.1);

        PID_Init(2,0.10,0.10,0.000,0.1);
        PID_Init(3,0.20,0.40,0.000,0.1);
        PID_Init(4,0.10 ,0.10,0.000,0.1);



    
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
        //if(i==3) writeSerialComln(String("Modo OFF - Referencia manual: ") + String(referenceCurrent) + String(" mA"));
        break;
        case ON_t: {
          if (checkLimits(i, getLastCurrentData(i) / 1000, 0, 0)) {
              // límite superado
          }


          //Esta variable solo cambia cuando el nuevo punto de la curva es del tipo step
          bool isNewStep = false;
          float curveVal = getCurveValue(i, &isNewStep);  // float, nombre distinto a aux
          //(i==3   )writeSerialComln(String("Canal ") + String(i) + String(" - Valor de curva: ") + String(curveVal) + String(" mA ") );
          if(feedforwardEnabled[i] && isNewStep){
              PID_EnableFeedforward(i);
          }




          if (curveVal != -1) {
              referenceCurrent = curveVal;
          } else {
              referenceCurrent = 0.0f;
          }
          break;
      }
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
       //dutyCycleAux = getDCDirecto(referenceCurrent); // Duty cycle directo
       //Uso el valor predecido de dutycycle para la referencia dada porelusuario
        if (feedforwardEnabled[i]) {
            dutyCycleAux = feedforward(referenceCurrent, i);

        } else {
            dutyCycleAux = getDCDirecto(referenceCurrent)*10.0f; // o la lógica que corresponda
             
        }

       

        //writeSerialComln(String("Modo NONE - Duty Cycle directo: ") + String(dutyCycleAux));
        break;
      default:   
        dutyCycleAux = MIN_DUTY; 
        //writeSerialComln(String("Modo desconocido - Duty Cycle: 0"));
        break;
    }


    //Clamp del duty para asegurar que esté dentro de los límites permitidos 
    //if (dutyCycleAux > MAX_DUTY) dutyCycleAux = MAX_DUTY;
    //if (dutyCycleAux < MIN_DUTY) dutyCycleAux = MIN_DUTY;




    // Aplicar el duty cycle actual (invertido)
    int pwmValue = (int)((dutyCycleAux) * pwmConfig[i].max_duty / 100.0f);

    

    
    
    ledcWrite(pwmConfig[i].channel,pwmValue); // Inicializar el PWM a 0 (apagado)  


  }


  

//Luego de actualizar el PWM, hago elprocesamiento pesado para evitar un posible delay

    for(int i = 0; i < NUMBER_OF_SENSORS; i++){
        if(identificaciones[i] != nullptr && identificaciones[i]->activa){
            guardarPuntoIdentificado(i, getLastCurrentData(i));
        }
    }

    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        if (calibracionActiva(i)) {
            calibracionUpdate(i, getLastCurrentData(i));
        }
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





modoFuncionamiento_t getModoFuncionamiento(int index){
    if(index<0 || index>=NUMBER_OF_ELECTRONIC_LOADS) return NONE;
    return modoFuncionamiento[index];
}


// Nuevo: setter para referencia manual de corriente (mA) usada por PID y curva en modo OFF
bool setCurrentReference_mA(float reference, int index){
    if(index < 0 || index >= NUMBER_OF_ELECTRONIC_LOADS) {
        writeSerialComln(String("ERROR: Index fuera de rango: ") + String(index));
        return false;
    }
    if(reference < 0.0f) {
        writeSerialComln(String("ERROR: Corriente negativa: ") + String(reference) + String(" mA"));
        return false;
    }
    
    // Si el setpoint cambió y el modo es PID, activar feedforward
    // Solo activar feedforward si está habilitado para este canal y estoa en modocorriente
    if (reference != currentReference_mA[index] && feedforwardEnabled[index] && getPIDMode(index) == 'i') {
        PID_EnableFeedforward(index);
    }

    currentReference_mA[index] = reference;
    return true;
}


//Mapea la referencia de corriente directa al duty cycle
float getDCDirecto(float ref){
  if(ref<0.0f) return 0.0f;
    return ref; // Convertir mA a %
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


bool setFeedforwardEnabled(bool enabled, int index) {
    if (index < 0 || index >= NUMBER_OF_ELECTRONIC_LOADS) return false;
    feedforwardEnabled[index] = enabled;
    return true;
}

bool getFeedforwardEnabled(bool *enabled, int index) {
    if (index < 0 || index >= NUMBER_OF_ELECTRONIC_LOADS) return false;
    *enabled = feedforwardEnabled[index];
    return feedforwardEnabled[index];
}




// ============================================================
// IDENTIFICACIÓN DE PLANTA — FUNCIONES PRIVADAS
// ============================================================


curve_mode_t  prev_curve_mode ;
modoFuncionamiento_t prev_control_mode ;
bool prev_feedforward_enabled ;

// Libera toda la memoria dinámica de una identificación y anula el puntero global
static void liberarIdentificacion(int pin) {
    if(pin < 0 || pin >= NUMBER_OF_SENSORS) {
        writeSerialComln(String("Error: pin inválido en procesarYGuardar: ") + String(pin));
        return;
    }

    Identificacion_t* id = identificaciones[pin];
    if (id == nullptr) return;

    if (id->dutyValues != nullptr) {
        free(id->dutyValues);
        id->dutyValues = nullptr;
    }
    
writeSerialComln(String("Liberando identificacion del canal ") + String(pin));
    if (id->mediciones != nullptr) {
        for (int i = 0; i < id->numPuntos; i++) {
            if (id->mediciones[i] != nullptr) {
                free(id->mediciones[i]);
                id->mediciones[i] = nullptr;
            }
        }
        free(id->mediciones);
        id->mediciones = nullptr;
    }

    if (id->resultados != nullptr) {
        free(id->resultados);
        id->resultados = nullptr;
    }

    free(id);
    identificaciones[pin] = nullptr;
}

// Procesa las mediciones crudas, guarda en NVS e imprime por consola.
// Por ahora los promedios se dejan en 0 — el cálculo se implementará después.
static void procesarYGuardar(int pin) {
    if (pin < 0 || pin >= NUMBER_OF_SENSORS) return;

    Identificacion_t* id = identificaciones[pin];
    if (id == nullptr) return;

    // Calcular promedios
    for (int i = 0; i < id->numPuntos; i++) {
        id->resultados[i].duty_percent = id->dutyValues[i];
        id->resultados[i].corriente_mA = calcularPromedio(id->mediciones[i], id->muestras_por_punto);
    }

    // Guardar en NVS con char[] para no fragmentar heap
    nvs_handle_t handle;
    char key[48];

    esp_err_t err = nvs_open(IDENT_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        writeSerialComln(String("Error NVS identificacion canal ") + String(pin));
        liberarIdentificacion(pin);
        return;
    }

    snprintf(key, sizeof(key), "ident_%d_pin",  pin); nvs_set_i32(handle, key, pin);
    snprintf(key, sizeof(key), "ident_%d_npts", pin); nvs_set_i32(handle, key, id->numPuntos);
    snprintf(key, sizeof(key), "ident_%d_ts",   pin); nvs_set_u32(handle, key, (uint32_t)id->timestampInicio);
    snprintf(key, sizeof(key), "ident_%d_mxpt", pin); nvs_set_i32(handle, key, id->muestras_por_punto);
    snprintf(key, sizeof(key), "ident_%d_data", pin);
    nvs_set_blob(handle, key, id->resultados, sizeof(PuntoIdentificado_t) * id->numPuntos);
    nvs_commit(handle);
    nvs_close(handle);

    // Restaurar modo antes de liberar
    PWMSetCurveMode(prev_curve_mode, pin);
    setControlMode(prev_control_mode, pin);
    setFeedforwardEnabled(prev_feedforward_enabled, pin);

    // Copiar resultados antes de liberar heap
    int numPuntos = id->numPuntos;
    float* dutyCopy      = (float*)malloc(sizeof(float) * numPuntos);
    float* resultadosCopy = (float*)malloc(sizeof(float) * numPuntos);
    if (dutyCopy && resultadosCopy) {
        for (int i = 0; i < numPuntos; i++) {
            dutyCopy[i]       = id->resultados[i].duty_percent;
            resultadosCopy[i] = id->resultados[i].corriente_mA;
        }
    }

    liberarIdentificacion(pin);

    // Imprimir con heap ya liberado
    writeSerialComln(String("=== IDENTIFICACION CANAL ") + String(pin) + String(" ==="));
    writeSerialComln(String("Puntos: ") + String(numPuntos));
    if (dutyCopy && resultadosCopy) {
        for (int i = 0; i < numPuntos; i++) {
            char buf[48];
            snprintf(buf, sizeof(buf), "  duty=%.1f%% -> %.2f mA", dutyCopy[i], resultadosCopy[i]);
            writeSerialComln(String(buf));
        }
    }
    writeSerialComln(String("=== FIN IDENTIFICACION CANAL ") + String(pin) + String(" ==="));

    if (dutyCopy)        free(dutyCopy);
    if (resultadosCopy)  free(resultadosCopy);
}
// Recibe una muestra de corriente, la almacena en el vector del punto actual
// e ignora las llamadas correspondientes al standby inicial.
static void guardarPuntoIdentificado(int pin, float corriente_mA) {
    Identificacion_t* id = identificaciones[pin];
    if (id == nullptr || !id->activa) return;

    // Ignorar las llamadas del standby inicial (duty=0, IDENT_TIEMPO_STANDBY_S segundos)
    int llamadasStandby = IDENT_TIEMPO_STANDBY_S * IDENT_MUESTRAS_POR_SEGUNDO;
    if (id->llamadasTotales < llamadasStandby) {
        id->llamadasTotales++;
        return;
    }

    // Guardar la muestra en el punto actual
    if (id->puntoActual < id->numPuntos &&
        id->muestraActual < id->muestras_por_punto) {

        id->mediciones[id->puntoActual][id->muestraActual] = corriente_mA;
        id->muestraActual++;
    }

    id->llamadasTotales++;

    // ¿Completamos todas las muestras del punto actual?
    if (id->muestraActual >= id->muestras_por_punto) {
        id->muestraActual = 0;
        id->puntoActual++;

        // ¿Era el último punto?
        if (id->puntoActual >= id->numPuntos) {
            writeSerialComln(String("Todas las muestras capturadas en canal ") + String(pin) + String(". Procesando..."));
            procesarYGuardar(pin);
            return;
            // procesarYGuardar llama a liberarIdentificacion al final
        }
    }
}


// ============================================================
// IDENTIFICACIÓN DE PLANTA — FUNCIÓN PÚBLICA
// ============================================================

bool identificacionInit(int pin, int numPuntos, int tiempo_por_punto_s) {



    // Validaciones
    if (pin < 0 || pin >= NUMBER_OF_SENSORS) {
        writeSerialComln(String("identificacionInit: pin invalido"));
        return false;
    }
    if (numPuntos < 2) {
        writeSerialComln(String("identificacionInit: se necesitan al menos 2 puntos"));
        return false;
    }
    if (tiempo_por_punto_s <= 0 || tiempo_por_punto_s > 60) {
        writeSerialComln(String("identificacionInit: tiempo por punto invalido"));
        return false;
    }

    // Si ya había una identificación activa en este canal, liberarla
    if (identificaciones[pin] != nullptr) {
        writeSerialComln(String("identificacionInit: liberando identificacion previa en canal ") + String(pin));
        liberarIdentificacion(pin);
    }


    prev_curve_mode = getCurveMode(pin);
    prev_control_mode = getModoFuncionamiento(pin);
    bool estado;
     if(getFeedforwardEnabled(&estado,pin)==true){
        prev_feedforward_enabled = estado;
     } else {
        prev_feedforward_enabled = false;
     }

    // ── 1. Preparar el canal: apagar PID, feedforward y curva ────────────
    PWMSetCurveMode(ON_t, pin);
    setControlMode(NONE, pin);
    setFeedforwardEnabled(false, pin);
    writeSerialComln(String("Canal ") + String(pin) + String(": PID off, FF off, curva on"));

    // ── 2. Crear y poblar la estructura de identificación ─────────────────
    Identificacion_t* id = (Identificacion_t*)malloc(sizeof(Identificacion_t));
    if (id == nullptr) {
        writeSerialComln(String("identificacionInit: sin memoria para estructura"));
        return false;
    }
    memset(id, 0, sizeof(Identificacion_t));

    id->numPuntos          = numPuntos;
    id->muestras_por_punto = tiempo_por_punto_s * IDENT_MUESTRAS_POR_SEGUNDO;
    id->puntoActual        = 0;
    id->muestraActual      = 0;
    id->llamadasTotales    = 0;
    id->activa             = false; // se activa al final, una vez todo listo
    id->pin                = pin;
    id->timestampInicio    = getCurrentEpoch();

    // ── 3. Calcular vector de duties ──────────────────────────────────────
    // N puntos entre IDENT_DUTY_MIN y IDENT_DUTY_MAX, ambos extremos incluidos
    id->dutyValues = (float*)malloc(sizeof(float) * numPuntos);
    if (id->dutyValues == nullptr) {
        free(id);
        writeSerialComln(String("identificacionInit: sin memoria para dutyValues"));
        return false;
    }
    float intervalo = (IDENT_DUTY_MAX - IDENT_DUTY_MIN) / (float)(numPuntos - 1);
    for (int i = 0; i < numPuntos; i++) {
        id->dutyValues[i] = IDENT_DUTY_MIN + i * intervalo;
    }

    // ── 4. Crear matriz de mediciones ─────────────────────────────────────
    id->mediciones = (float**)malloc(sizeof(float*) * numPuntos);
    if (id->mediciones == nullptr) {
        free(id->dutyValues);
        free(id);
        writeSerialComln(String("identificacionInit: sin memoria para mediciones[]"));
        return false;
    }
    memset(id->mediciones, 0, sizeof(float*) * numPuntos);

    for (int i = 0; i < numPuntos; i++) {
        id->mediciones[i] = (float*)malloc(sizeof(float) * id->muestras_por_punto);
        if (id->mediciones[i] == nullptr) {
            for (int j = 0; j < i; j++) free(id->mediciones[j]);
            free(id->mediciones);
            free(id->dutyValues);
            free(id);
            writeSerialComln(String("identificacionInit: sin memoria para mediciones[") + String(i) + String("]"));
            return false;
        }
        memset(id->mediciones[i], 0, sizeof(float) * id->muestras_por_punto);
    }

    // ── 5. Crear vector de resultados ─────────────────────────────────────
    id->resultados = (PuntoIdentificado_t*)malloc(sizeof(PuntoIdentificado_t) * numPuntos);
    if (id->resultados == nullptr) {
        for (int i = 0; i < numPuntos; i++) free(id->mediciones[i]);
        free(id->mediciones);
        free(id->dutyValues);
        free(id);
        writeSerialComln(String("identificacionInit: sin memoria para resultados"));
        return false;
    }
    memset(id->resultados, 0, sizeof(PuntoIdentificado_t) * numPuntos);

    // ── 6. Crear la curva de identificación en simuladorCurvas ───────────
    // IDs 100..104 reservados para identificación (uno por canal)
    int curveId = createCurve(100 + pin);
    if (curveId < 0) {
        for (int i = 0; i < numPuntos; i++) free(id->mediciones[i]);
        free(id->mediciones);
        free(id->dutyValues);
        free(id->resultados);
        free(id);
        writeSerialComln(String("identificacionInit: error al crear curva en simuladorCurvas"));
        return false;
    }
    id->curveId = curveId;

    // Punto 0: standby en duty=0 durante IDENT_TIEMPO_STANDBY_S segundos
    // getDCDirecto hace duty = 10*ref → para duty=0, ref=0
    addPointToCurve(curveId, IDENT_TIEMPO_STANDBY_S, 0.0f, STEP);

    // Puntos 1..N: un punto por cada duty del vector
    // getDCDirecto hace duty = 10*ref → ref = duty/10
    int tiempoAcumulado = IDENT_TIEMPO_STANDBY_S;
    for (int i = 0; i < numPuntos; i++) {
        tiempoAcumulado += tiempo_por_punto_s;
        float refValue = id->dutyValues[i] / 10.0f; // compensar el *10 de getDCDirecto
        addPointToCurve(curveId, tiempoAcumulado, refValue, STEP);
    }

    // ── 7. Activar la curva en el canal ───────────────────────────────────
    // curveMode ON + modoFuncionamiento NONE → getCurveValue provee la referencia
    // y getDCDirecto la convierte en duty directo, sin PID ni feedforward
    if (!asociarCurvaAPin(curveId, pin)) {
        deleteCurve(curveId);
        for (int i = 0; i < numPuntos; i++) free(id->mediciones[i]);
        free(id->mediciones);
        free(id->dutyValues);
        free(id->resultados);
        free(id);
        writeSerialComln(String("identificacionInit: error al asociar curva al pin ") + String(pin));
        return false;
    }
    PWMSetCurveMode(ON_t, pin);

    // ── 8. Registrar y activar ────────────────────────────────────────────
    id->activa          = true;
    identificaciones[pin] = id;

    writeSerialComln(
        String("Identificacion iniciada: canal=") + String(pin) +
        String(" | puntos=") + String(numPuntos) +
        String(" | tiempo/punto=") + String(tiempo_por_punto_s) + String("s") +
        String(" | muestras/punto=") + String(id->muestras_por_punto) +
        String(" | standby=") + String(IDENT_TIEMPO_STANDBY_S) + String("s")
    );

    return true;
}
 
void cargarEImprimirIdentificacionNVS(int pin) {
    if (pin < 0 || pin >= NUMBER_OF_SENSORS) {
        writeSerialComln(String("cargarEImprimirIdentificacionNVS: pin invalido"));
        return;
    }

    nvs_handle_t handle;
    char nsKey[32];
    snprintf(nsKey, sizeof(nsKey), "ident_%d", pin);

    esp_err_t err = nvs_open(IDENT_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        writeSerialComln(String("No hay identificacion guardada para canal ") + String(pin));
        return;
    }

    // Leer metadatos
    int32_t npts = 0, mxpt = 0;
    uint32_t ts = 0;
    nvs_get_i32(handle, (String(nsKey) + "_npts").c_str(), &npts);
    nvs_get_u32(handle, (String(nsKey) + "_ts").c_str(),   &ts);
    nvs_get_i32(handle, (String(nsKey) + "_mxpt").c_str(), &mxpt);

    if (npts <= 0 || npts > 100) {
        writeSerialComln(String("Datos de identificacion invalidos en NVS para canal ") + String(pin));
        nvs_close(handle);
        return;
    }

    // Leer blob de resultados
    size_t blobSize = sizeof(PuntoIdentificado_t) * npts;
    PuntoIdentificado_t* resultados = (PuntoIdentificado_t*)malloc(blobSize);
    if (resultados == nullptr) {
        writeSerialComln(String("Sin memoria para leer identificacion de NVS"));
        nvs_close(handle);
        return;
    }

    err = nvs_get_blob(handle, (String(nsKey) + "_data").c_str(), resultados, &blobSize);
    nvs_close(handle);

    if (err != ESP_OK) {
        writeSerialComln(String("Error al leer blob de identificacion canal ") + String(pin));
        free(resultados);
        return;
    }

    // Imprimir
    writeSerialComln(String("=============================="));
    writeSerialComln(String("IDENTIFICACION CANAL ") + String(pin));
    writeSerialComln(String("Timestamp: ") + String(ts));
    writeSerialComln(String("Puntos: ") + String(npts));
    writeSerialComln(String("Muestras/punto: ") + String(mxpt));
    writeSerialComln(String("duty(%) -> corriente(mA)"));
    writeSerialComln(String("------------------------------"));
    for (int i = 0; i < npts; i++) {
        char buf[48];
        snprintf(buf, sizeof(buf), "  %.1f%% -> %.2f mA",
                 resultados[i].duty_percent,
                 resultados[i].corriente_mA);
        writeSerialComln(String(buf));
    }
    writeSerialComln(String("=============================="));

    free(resultados);
}


 bool isIdentificacionRunning(int pin) {
    if (pin < 0 || pin >= NUMBER_OF_SENSORS) return false;
    Identificacion_t* id = identificaciones[pin];
    return (id != nullptr && id->activa);
}

static float calcularPromedio(float* muestras, int total) {
    if (muestras == nullptr || total <= 0) return 0.0f;
    int inicio = total / 2;
    float suma = 0.0f;
    for (int i = inicio; i < total; i++) {
        suma += muestras[i];
    }
    return suma / (float)(total - inicio);
}




PuntoIdentificado_t* getIdentificacionNVS(int pin, int* numPuntos) {
    if (pin < 0 || pin >= NUMBER_OF_SENSORS || numPuntos == nullptr) return nullptr;

    nvs_handle_t handle;
    char key[48];

    esp_err_t err = nvs_open(IDENT_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return nullptr;

    int32_t npts = 0;
    snprintf(key, sizeof(key), "ident_%d_npts", pin);
    if (nvs_get_i32(handle, key, &npts) != ESP_OK || npts <= 0 || npts > 100) {
        nvs_close(handle);
        return nullptr;
    }

    PuntoIdentificado_t* datos = (PuntoIdentificado_t*)malloc(sizeof(PuntoIdentificado_t) * npts);
    if (datos == nullptr) {
        nvs_close(handle);
        return nullptr;
    }

    size_t blobSize = sizeof(PuntoIdentificado_t) * npts;
    snprintf(key, sizeof(key), "ident_%d_data", pin);
    if (nvs_get_blob(handle, key, datos, &blobSize) != ESP_OK) {
        free(datos);
        nvs_close(handle);
        return nullptr;
    }

    nvs_close(handle);
    *numPuntos = npts;
    return datos;
}
