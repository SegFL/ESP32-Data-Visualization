

#include "simuladorCurvas.h"
#include "../../modulos/time/time.h"
curve_t** curveArray=NULL;
int curveArraySize=0;

#define NUMERO_PASOS_XSEG 5//Cantidad de veces que se llamaa getcurvevalue por segundo t=200ms==>5 veces por segundo
#define PERIODO_INTERRUPCION 0.2f // segundos (200ms)
#define MAX_PINES 4
//Variables usadas para el modo lineal por incrementos
static int index_pasos,numero_pasos=0; //Contador de pasos dentro del segmento
static float delta_v,delta_t,pendiente,incremento=0.0f;
curve_t* pinToCurve[MAX_PINES];
void calcular_incrementos(bool puntro_nuevo,curve_t* curve);
int getAvailableId();
curve_t** newCurveArray(int size){
    curve_t** newArray=(curve_t**)malloc(sizeof(curve_t*)*size);
    if (newArray == NULL) {
        // Manejar error: No se pudo asignar memoria
        return NULL;
    }
    for(int i=0;i<size;i++){
        newArray[i]=NULL;
    }
    for(int i=0;i<MAX_PINES;i++){
        pinToCurve[i]=NULL;
    }
    return newArray;
}
int createCurve(int pin) {
    // Verificar que el array esté inicializado
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas no inicializado"));
        return -1;
    }
    
    // Buscar la próxima posición disponible
    int nextPosition = -1;
    for(int i = 0; i < curveArraySize; i++) {
        if(curveArray[i] == NULL) {
            nextPosition = i;
            break;
        }
    }
    
    
    // Si no hay posición disponible
    if(nextPosition == -1) {
        writeSerialComln(String("Error: No hay espacio disponible en el array de curvas"));
        return -1;
    }
    
    // CORRECCIÓN: Verificar memoria disponible antes de crear
    if(ESP.getFreeHeap() < 2048) { // Necesitamos al menos 2KB libres
        writeSerialComln(String("Error: Memoria insuficiente para crear curva"));
        return -1;
    }
    
    curve_t *curve = (curve_t *)malloc(sizeof(curve_t));
    if (curve == NULL) {
        writeSerialComln(String("Error al crear la curva"));
        return -1;
    }

    curve->point = (point_t *)malloc( sizeof(point_t)*10);
    if (curve->point == NULL) {
        free(curve);
        writeSerialComln(String("Error al crear los puntos de la curva"));
        return -1;
    }
    curve->size = 10;
    curve->Imax = 0;
    curve->Imin = 0;
    curve->Vmax = 0;
    curve->Vmin = 0;
    curve->Pmax = 0;
    curve->Pmin = 0;
    curve->enabled = false;
    curve->pin = pin;
    curve->timestamp =0; // CORRECCIÓN: Inicializar con tiempo actual
    curve->point[0].tiempo = 0;
    curve->point[0].value = 0.0f;
    curve->point[0].type = STEP; // o LINEAR, pero definido
    curve->contador = 1;
    curve->currentIndex=0;  
    curve->id=getAvailableId(); // Asignar un ID único

    
    // CORRECCIÓN: Inicializar todos los puntos restantes para evitar valores basura
    for(int i = 1; i < curve->size; i++) {
        curve->point[i].tiempo = 0;
        curve->point[i].value = 0.0f;
        curve->point[i].type = STEP;
    }
    
    // Asignar la curva a la posición disponible en el array
    curveArray[nextPosition] = curve;
    
    writeSerialComln(String("Curva creada correctamente con id ") + String(curve->id) + 
                     String(" - Size: ") + String(curve->size) + String(", Contador: ") + String(curve->contador));
    return nextPosition; // Devolver el ID de la curva (posición en el array)
}

bool validateCurve(curve_t *curve) {
    // NUEVA: Función para validar la integridad de una curva
    if(!curve) {
        writeSerialComln(String("VALIDACIÓN: curve es NULL"));
        return false;
    }
    
    if(!curve->point) {
        writeSerialComln(String("VALIDACIÓN: curve->point es NULL"));
        return false;
    }
    
    if(curve->size <= 0) {
        writeSerialComln(String("VALIDACIÓN: size inválido: ") + String(curve->size));
        return false;
    }
    
    if(curve->contador < 0 || curve->contador > curve->size) {
        writeSerialComln(String("VALIDACIÓN: contador fuera de límites: ") + String(curve->contador) + "/" + String(curve->size));
        return false;
    }
    
    return true;
}

bool initCurve(curve_t *curve) {
    if (!curve) return false;
    curve->enabled = true;
    curve->timestamp = millis();
    return true;
}

//Devuelve el valor del punto actual de la curva
//Si no hay puntos, devuelve -1
//Recive comoparametro elnumero de IDde la curva en el array
int getCurveValue(int pin) {
    bool punto_nuevo=false;
    if (curveArray == NULL) {
        writeSerialComln("Error: Array de curvas no inicializado");
        return -1;
    }
    if (pin < 0 || pin >= MAX_PINES) {
        writeSerialComln("Error: Curva no válida");
        return -1;
    }

    //Curva asociada al pin
    curve_t *curve = pinToCurve[pin];
    if (!curve || !curve->point || curve->contador <= 0) {
        return -1; // no hay puntos válidos
    }
    if (!curve->enabled) {
        return -1; // curva deshabilitada
    }

    unsigned long currentTime = getCurrentEpoch();
    writeSerialComln(String("Current Time: ") + String(currentTime) +
                     ", Curve Timestamp: " + String(curve->timestamp));

    // Avanzar solo si no llegamos al último punto
    if (curve->currentIndex < curve->contador -1) {
        unsigned long nextPointTime = curve->timestamp + curve->point[curve->currentIndex + 1].tiempo;

        if (currentTime >= nextPointTime) {
            curve->currentIndex++;
            punto_nuevo=true;
        }
    }else{

        // Si llegamos al último punto, deshabilitar la curva
        curve->enabled = false;
        writeSerialComln(String("Curva del pin") + String(pin) + String(" finalizada y deshabilitada."));
        return 0.0;
    }


    calcular_incrementos(punto_nuevo,curve);



    if(curve->point[curve->currentIndex].type==LINEAR){
        writeSerialComln(String("Curva en modo LINEAL"));
        //Devuelvo el valor anterior mas el incremento
        if(index_pasos<numero_pasos) {
            index_pasos++;

        }else{
            
            //Si ya llegue al numero de pasos, devuelvo el valor actual
            return curve->point[curve->currentIndex].value;
        }

        return curve->point[curve->currentIndex-1].value+(index_pasos)*incremento;
    }
    // Devolver el valor actual (sin pasarse del último)
    return curve->point[curve->currentIndex].value;
}

void calcular_incrementos(bool punto_nuevo,curve_t *curve) {

        if(punto_nuevo==true){
        //Seleccion del tipo de punto (LINEAL o STEP)
        if(curve->point[curve->currentIndex].type==LINEAR){
            //Si elpunto es nuevo calculo los incrementos
            index_pasos=0;//Reinicio el contador de pasos
            delta_v=(curve->point[curve->currentIndex].value - curve->point[curve->currentIndex - 1].value);
            delta_t=(curve->point[curve->currentIndex].tiempo - curve->point[curve->currentIndex - 1].tiempo);
            numero_pasos=delta_t*NUMERO_PASOS_XSEG;
            if(delta_t!=0){
                pendiente=delta_v/delta_t;
            }else{
                pendiente=0;
            }
            incremento=pendiente*PERIODO_INTERRUPCION; //Valor a incrementar en cada ciclo
        }else{
            //Si es STEP no hay incrementos
            incremento=0;
        }

        writeSerialComln(String("Curva ") + String(" avanzó al punto index: ") + String(curve->currentIndex) +
                         String(" [Tiempo: ") + String(curve->point[curve->currentIndex].tiempo) +
                         String(", Valor: ") + String(curve->point[curve->currentIndex].value) +
                         String(", Tipo: ") + (curve->point[curve->currentIndex].type == LINEAR ? "LINEAR" : "STEP") + String("]")+
                         String(" incremento:")+String(incremento)+String(" numero_pasos:")+String(numero_pasos)+
                         String(" delta_t:")+String(delta_t)+String(" delta_v:")+String(delta_v)+
                         String(" pendiente:")+String(pendiente)
                         );
        punto_nuevo=false;
    }

}






curve_t* addPoint(curve_t *curve, int tiempo, float value,aproximation_point_type_t type=STEP) {
    // Verifica que curve y curve->point no sean NULL
    if (!curve || !curve->point) return NULL;

    // El tiempo debe ser mayor al último punto agregado
    if (tiempo <= curve->point[curve->contador - 1].tiempo) {
        writeSerialComln(String("Error: Tiempo invalido"));
        return NULL;
    }

    // Si se llena el array, realoca espacio
    if (curve->contador >= curve->size) {
        point_t* new_points = (point_t*)realloc(curve->point, sizeof(point_t) * (curve->size + 10));
        if (new_points == NULL) {
            writeSerialComln(String("Error: No se pudo realocar memoria para puntos"));
            return NULL;
        }
        curve->point = new_points;
        curve->size += 10;
        
    }

    // Asigna el valor del nuevo punto
    curve->point[curve->contador].tiempo = tiempo;
    curve->point[curve->contador].value = value;
    curve->point[curve->contador].type = type;
    curve->contador++;

    return curve;
}

// NUEVA: Función encapsulada para agregar puntos por ID de curva
int addPointToCurve(int curveId, int tiempo, float value,aproximation_point_type_t type) {
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas no inicializado"));
        return -1;
    }
    
    if (curveId < 0 || curveId >= curveArraySize) {
        writeSerialComln(String("Error: Curva no valida"));
        return -1;
    }
    
    curve_t *curve = curveArray[curveId];
    if (curve == NULL) {
        writeSerialComln(String("Error: Curva no existe"));
        return -1;
    }
    
    curve_t* result = addPoint(curve, tiempo, value,type);
    if (result == NULL) {
        return -1;
    }
    
    writeSerialComln(String("Punto agregado a curva ") + String(curveId) + String(": [") + String(tiempo) + String(",") + String(value) + String("]"));
    return 0; // Éxito
}

//Imprime por consola una curva en particular
//La app no toma como valida una curva enviada por esta funcion
void printCurves() { 
    if (curveArray == NULL) {
        writeSerialComln("Error: Array de curvas es NULL");
        return;
    }

    writeSerialComln(String("curveArraySize: ") + String(curveArraySize));
    for (int i = 0; i < curveArraySize; i++) {
        if (curveArray[i] != NULL) {
            printCurve(curveArray[i], i);
        }
    }
}




//Envia por puerto serie las curvas en un formato entendible por la App
void sendCurves(){
    // CORRECCIÓN: Optimizada para reducir uso de memoria con String
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas es NULL"));
        return;
    }

    /*Para envio de dato a la App*/
    for(int i=0;i<curveArraySize;i++){
        if(curveArray[i]!=NULL){
            //3=Indicador de que se esta enviando una curva
            //"3,pin,contador,[tiempo,valor],[tiempo,valor],..."
            writeSerialCom( String('3')+','+String(curveArray[i]->pin)+','+String(curveArray[i]->contador));

            // Limitar la cantidad de puntos a imprimir para evitar overflow
            int maxPrint = min(curveArray[i]->contador, 20); // Máximo 20 puntos
            for(int j=0;j<maxPrint;j++){
                char buffer[50];
                sprintf(buffer, ",[%d,%d]", curveArray[i]->point[j].tiempo, curveArray[i]->point[j].value);
                writeSerialCom(String(buffer));
            }
            writeSerialComln(""); // Nueva línea al final

                
        }else{
            writeSerialComln(String("Curva ") + String(i) + String(": VACIA"));
        }

    }
    /* Esto es para mostrar para el usuario
    for(int i=0;i<size;i++){
        if(curveArray[i]!=NULL){
            writeSerialComln(String("Curva ") + String(i));
            writeSerialComln(String("Pin: ") + String(curveArray[i]->pin));
            writeSerialComln(String("Cantidad de puntos: ") + String(curveArray[i]->contador));           
            writeSerialComln(String("Puntos:[Tiempo, Valor]"));
            
            // Limitar la cantidad de puntos a imprimir para evitar overflow
            int maxPrint = min(curveArray[i]->contador, 20); // Máximo 20 puntos
            for(int j=0;j<maxPrint;j++){
                char buffer[50];
                sprintf(buffer, "\t[%d,%d]", curveArray[i]->point[j].tiempo, curveArray[i]->point[j].value);
                writeSerialComln(String(buffer));
            }
            if(curveArray[i]->contador > 20) {
                writeSerialComln(String("... y ") + String(curveArray[i]->contador - 20) + String(" puntos más"));
            }
                
        }
            
    }
        */
}

void simuladorCurvasInit(int size) {
    curveArray = newCurveArray(size);
    if (curveArray == NULL) {
        writeSerialComln(String("Error al crear el arreglo de curvas"));
        
    }else{
        writeSerialComln(String("Arreglo de curvas inicializado correctamente"));
        curveArraySize = size;
    }
    
    return;
}

// NUEVA: Función para eliminar una curva por ID
bool deleteCurve(int curveId) {
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas no inicializado"));
        return false;
    }
    
    if (curveId < 0 || curveId >= curveArraySize) {
        writeSerialComln(String("Error: Curva no valida"));
        return false;
    }
    
    curve_t *curve = curveArray[curveId];
    if (curve == NULL) {
        writeSerialComln(String("Error: Curva no existe"));
        return false;
    }
    
    // Liberar memoria de los puntos
    if (curve->point != NULL) {
        free(curve->point);
    }
    
    // Liberar memoria de la curva
    free(curve);
    
    // Marcar la posición como disponible
    curveArray[curveId] = NULL;
    
    writeSerialComln(String("Curva ") + String(curveId) + String(" eliminada correctamente"));
    return true;
}

// NUEVA: Función para obtener la cantidad de curvas activas
int getCurveCount() {
    if(curveArray == NULL) {
        return 0;
    }
    
    int count = 0;
    for(int i = 0; i < curveArraySize; i++) {
        if(curveArray[i] != NULL) {
            count++;
        }
    }
    
    return count;
}

// Habilita o deshabilita una curva
bool enableCurve(int curveId, int pin) {
    if (curveArray == NULL) {writeSerialComln("Error: Array de curvas no inicializado");return false;}
    if (curveId < 0 || curveId >= curveArraySize) {writeSerialComln("Error: Curva no válida");return false;}
    if (pin < 0 || pin >= MAX_PINES) {writeSerialComln("Error: Pin no válido");return false;}
    if (curveArray[curveId] == NULL) {writeSerialComln("Error: Curva no existe");return false;}

    curve_t *curve = curveArray[curveId];

    // Toggle de estado
    if (curve->enabled) {
        // Si estaba habilitada → deshabilitar
        curve->enabled = false;
        curve->timestamp = 0;
        curve->currentIndex = 0;
        pinToCurve[pin]=NULL; // Desaociar curva al pin
        writeSerialComln(String("Curva ") + String(curveId) + " deshabilitada.");
        return false;
    } else {
        // Si estaba deshabilitada → habilitar
        unsigned long t0 = getCurrentEpoch();
        curve->enabled = true;
        curve->timestamp = t0;
        curve->currentIndex = 0;

        // Verificar si la curva ya está asociada a otro pin
        for (int i = 0; i < MAX_PINES; i++) {
            if (pinToCurve[i] == curveArray[curveId]) {
                writeSerialComln(String("Error: La curva ") + String(curveId) +
                                " ya está asociada al pin " + String(i));
                curve->enabled = false; // revertir habilitación
                return false;
            }
        }

        curve->pin = pin; // asignar pin cada vez que se habilita
        pinToCurve[pin]=curveArray[curveId]; // Asociar curva al pin
        writeSerialComln(String("Curva ") + String(curveId) + " habilitada en pin " + String(pin));
        return true;
    }
}



//Nombre de la curva (key)<curveX> y el ID de la curva en el array
void saveCurveNVS(const char* key, int curveId) {
    nvs_handle_t handle;
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas no inicializado"));
        return;
    }
    if (curveId < 0 || curveId >= curveArraySize) {
        writeSerialComln(String("Error: Curva no valida"));
        return;
    }
    curve_t *curve = curveArray[curveId];
    if (curve == NULL) {
        writeSerialComln(String("Error: Curva no existe"));
        return;
    }
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        writeSerialComln("Error al abrir NVS");
        return;
    }

    // Guardar enteros y banderas
    nvs_set_i32(handle, (String(key) + "_Imax").c_str(), curve->Imax);
    nvs_set_i32(handle, (String(key) + "_Imin").c_str(), curve->Imin);
    nvs_set_i32(handle, (String(key) + "_Vmax").c_str(), curve->Vmax);
    nvs_set_i32(handle, (String(key) + "_Vmin").c_str(), curve->Vmin);
    nvs_set_i32(handle, (String(key) + "_Pmax").c_str(), curve->Pmax);
    nvs_set_i32(handle, (String(key) + "_Pmin").c_str(), curve->Pmin);
    nvs_set_i32(handle, (String(key) + "_contador").c_str(), curve->contador);
    nvs_set_i32(handle, (String(key) + "_size").c_str(), curve->size);
    nvs_set_i32(handle, (String(key) + "_pin").c_str(), curve->pin);
    nvs_set_u32(handle, (String(key) + "_timestamp").c_str(), curve->timestamp);
    nvs_set_u8(handle, (String(key) + "_enabled").c_str(), curve->enabled ? 1 : 0);

    // Guardar los puntos como BLOB
    if (curve->point != NULL && curve->size > 0) {
        nvs_set_blob(handle, (String(key) + "_points").c_str(),
                     curve->point, sizeof(point_t) * curve->size);
    }

    // Confirmar
    err = nvs_commit(handle);
    if (err != ESP_OK) {
        writeSerialComln("Error al hacer commit en NVS");
    }

    nvs_close(handle);
}


bool loadCurveNVS(const char* key) {
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas no inicializado"));
        return false;
    }
    int i=0;
    for(i=0;i<curveArraySize;i++){
        if(curveArray[i]==NULL){
            writeSerialComln(String("Cargando curva en posición ") + String(i));
            curveArray[i] = (curve_t *)malloc(sizeof(curve_t));
            if (curveArray[i] == NULL) {
                writeSerialComln(String("Error al crear la curva"));
                return false;
            }
            break;
        }
    }
    curve_t *curve = curveArray[i];
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        writeSerialComln("Error al abrir NVS para lectura");
        return false;
    }

    // Leer enteros y banderas
    nvs_get_i32(handle, (String(key) + "_Imax").c_str(), &curve->Imax);
    nvs_get_i32(handle, (String(key) + "_Imin").c_str(), &curve->Imin);
    nvs_get_i32(handle, (String(key) + "_Vmax").c_str(), &curve->Vmax);
    nvs_get_i32(handle, (String(key) + "_Vmin").c_str(), &curve->Vmin);
    nvs_get_i32(handle, (String(key) + "_Pmax").c_str(), &curve->Pmax);
    nvs_get_i32(handle, (String(key) + "_Pmin").c_str(), &curve->Pmin);
    nvs_get_i32(handle, (String(key) + "_contador").c_str(), &curve->contador);
    nvs_get_i32(handle, (String(key) + "_size").c_str(), &curve->size);
    nvs_get_i32(handle, (String(key) + "_pin").c_str(), &curve->pin);
    nvs_get_u32(handle, (String(key) + "_timestamp").c_str(), &curve->timestamp);
    uint8_t enabled;
    nvs_get_u8(handle, (String(key) + "_enabled").c_str(), &enabled);
    curve->enabled = (enabled != 0);

    // Leer los puntos como BLOB
    if (curve->size > 0) {
        size_t blob_size = sizeof(point_t) * curve->size;
        curve->point = (point_t*) malloc(blob_size);
        if (curve->point == NULL) {
            writeSerialComln("Error al reservar memoria para points");
            nvs_close(handle);
            return false;
        }
        err = nvs_get_blob(handle, (String(key) + "_points").c_str(),
                           curve->point, &blob_size);
        if (err != ESP_OK) {
            writeSerialComln("Error al leer blob de points");
            free(curve->point);
            curve->point = NULL;
            nvs_close(handle);
            return false;
        }
    } else {
        curve->point = NULL;
    }

    nvs_close(handle);
    return true;
}





void printCurve(curve_t* c, int index) {
    if (c == NULL) {
        writeSerialComln("Curva NULL");
        return;
    }

    writeSerialComln("===================================");
    if (index >= 0) {
        writeSerialComln(String("Curva ") + String(index));
    }
    writeSerialComln(String("Pin asociado: ") + String(c->pin));
    writeSerialComln(String("Estado: ") + String(c->enabled ? "Habilitada" : "Deshabilitada"));
    writeSerialComln(String("Timestamp: ") + String(c->timestamp));
    writeSerialComln(String("Cantidad de puntos: ") + String(c->contador));
    writeSerialComln("Puntos: [Tiempo, Valor, Tipo]");

    int lastIndex = c->contador - 1;
    bool first = true;

    for (int j = 0; j <= lastIndex; j++) {
        int t = c->point[j].tiempo;
        float v = c->point[j].value;
        aproximation_point_type_t type = c->point[j].type;

        char typeChar = '?';
        switch (type) {
            case LINEAR:  typeChar = 'L'; break;
            case STEP:    typeChar = 'S'; break;
            case S_CURVE: typeChar = 'C'; break;
        }

        if (j == 0 || j == lastIndex || (t != 0 || v != 0)) {
            char buffer[60];
            sprintf(buffer, "[%d,%.2f,%c]", t, v, typeChar);

            if (!first) {
                writeSerialCom(",");
            }
            writeSerialCom(String(buffer));
            first = false;
        }
    }
    writeSerialComln("");
}


bool printCurveFromNvs(const char* key) {
    curve_t tempCurve;
    memset(&tempCurve, 0, sizeof(curve_t));

    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        writeSerialComln("Error al abrir NVS para lectura");
        return false;
    }

    // Verificar si existe la clave "_size" → si no existe, la curva no está guardada
    int size = 0;
    err = nvs_get_i32(handle, (String(key) + "_size").c_str(), &size);
    if (err != ESP_OK || size <= 0) {
        nvs_close(handle);
        return false;
    }

    // Leer metadatos principales
    nvs_get_i32(handle, (String(key) + "_Imax").c_str(), &tempCurve.Imax);
    nvs_get_i32(handle, (String(key) + "_Imin").c_str(), &tempCurve.Imin);
    nvs_get_i32(handle, (String(key) + "_Vmax").c_str(), &tempCurve.Vmax);
    nvs_get_i32(handle, (String(key) + "_Vmin").c_str(), &tempCurve.Vmin);
    nvs_get_i32(handle, (String(key) + "_Pmax").c_str(), &tempCurve.Pmax);
    nvs_get_i32(handle, (String(key) + "_Pmin").c_str(), &tempCurve.Pmin);
    nvs_get_i32(handle, (String(key) + "_contador").c_str(), &tempCurve.contador);
    nvs_get_i32(handle, (String(key) + "_size").c_str(), &tempCurve.size);
    nvs_get_i32(handle, (String(key) + "_pin").c_str(), &tempCurve.pin);
    nvs_get_u32(handle, (String(key) + "_timestamp").c_str(), &tempCurve.timestamp);

    uint8_t enabled = 0;
    nvs_get_u8(handle, (String(key) + "_enabled").c_str(), &enabled);
    tempCurve.enabled = (enabled != 0);

    // Leer puntos si existen
    if (tempCurve.size > 0) {
        size_t blob_size = sizeof(point_t) * tempCurve.size;
        tempCurve.point = (point_t*) malloc(blob_size);
        if (tempCurve.point != NULL) {
            err = nvs_get_blob(handle, (String(key) + "_points").c_str(),
                               tempCurve.point, &blob_size);
            if (err != ESP_OK) {
                free(tempCurve.point);
                tempCurve.point = NULL;
            }
        }
    } else {
        tempCurve.point = NULL;
    }

    nvs_close(handle);

    // Imprimir curva
    writeSerialComln("===================================");
    writeSerialComln(String("Curva ") + key);
    writeSerialComln(String("Pin asociado: ") + String(tempCurve.pin));
    writeSerialComln(String("Estado: ") + (tempCurve.enabled ? "Habilitada" : "Deshabilitada"));
    writeSerialComln(String("Timestamp: ") + String(tempCurve.timestamp));
    writeSerialComln(String("Cantidad de puntos: ") + String(tempCurve.size));
    writeSerialComln("Puntos: [Tiempo, Valor, Tipo]");

    if (tempCurve.point != NULL) {
        for (int i = 0; i < tempCurve.size; i++) {
            writeSerialComln("[" + String(tempCurve.point[i].tiempo) + "," +
                             String(tempCurve.point[i].value, 2) + "," +
                             String(tempCurve.point[i].type) + "]");
        }
        free(tempCurve.point);
    }

    return true;
}



void printPinToCurve(){
    writeSerialComln("Pines y curvas asociadas:");
    for(int i=0;i<MAX_PINES;i++){
        if(pinToCurve[i]!=NULL){
            writeSerialComln(String("Pin ") + String(i) + String(" -> Curva en array con pin: ") + String(pinToCurve[i]->pin));
        }else{
            writeSerialComln(String("Pin ") + String(i) + String(" -> No tiene curva asociada"));
        }
    }
}



int getAvailableId() {
    if (curveArray == NULL || curveArraySize <= 0)
        return -1;

    int id = 0;
    bool idOcupado;

    while (true) {
        idOcupado = false;

        // Verificar si alguna curva tiene este ID
        for (int i = 0; i < curveArraySize; i++) {
            if (curveArray[i] != NULL && curveArray[i]->id == id) {
                idOcupado = true;
                break;
            }
        }

        // Si no está ocupado, lo devolvemos
        if (!idOcupado)
            return id;

        id++; // probar el siguiente ID
    }

    // En teoría nunca llega acá
    return -1;
}

