

#include "simuladorCurvas.h"
#include "../../modulos/time/time.h"
curve_t** curveArray=NULL;
int curveArraySize=0;



curve_t** newCurveArray(int size){
    curve_t** newArray=(curve_t**)malloc(sizeof(curve_t*)*size);
    if (newArray == NULL) {
        // Manejar error: No se pudo asignar memoria
        return NULL;
    }
    for(int i=0;i<size;i++){
        newArray[i]=NULL;
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
    curve->point[0].value = 0;
    curve->contador = 1;
    curve->currentIndex=0;  

    
    // CORRECCIÓN: Inicializar todos los puntos restantes para evitar valores basura
    for(int i = 1; i < curve->size; i++) {
        curve->point[i].tiempo = 0;
        curve->point[i].value = 0;
    }
    
    // Asignar la curva a la posición disponible en el array
    curveArray[nextPosition] = curve;
    
    writeSerialComln(String("Curva creada correctamente en posición ") + String(nextPosition) + 
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
//No implemente los limites seteados en el createCurve(): Antes del return deveria validas los valores maximos y minimos
//Recive comoparametro elnumero de IDde la curva en el array
int getCurveValue(int curveId) {
    if (curveArray == NULL) {
        writeSerialComln("Error: Array de curvas no inicializado");
        return -1;
    }
    if (curveId < 0 || curveId >= curveArraySize) {
        writeSerialComln("Error: Curva no válida");
        return -1;
    }

    curve_t *curve = curveArray[curveId];
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
    if (curve->currentIndex < curve->contador - 1) {
        unsigned long nextPointTime = curve->timestamp + curve->point[curve->currentIndex + 1].tiempo;

        if (currentTime >= nextPointTime) {
            curve->currentIndex++;
        }
    }else{
        // Si llegamos al último punto, deshabilitar la curva
        curve->enabled = false;
        writeSerialComln(String("Curva ") + String(curveId) + String(" finalizada y deshabilitada."));
    }

    // Devolver el valor actual (sin pasarse del último)
    return curve->point[curve->currentIndex].value;
}




curve_t* addPoint(curve_t *curve, int tiempo, int value) {
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
    curve->contador++;

    return curve;
}

// NUEVA: Función encapsulada para agregar puntos por ID de curva
int addPointToCurve(int curveId, int tiempo, int value) {
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
    
    curve_t* result = addPoint(curve, tiempo, value);
    if (result == NULL) {
        return -1;
    }
    
    writeSerialComln(String("Punto agregado a curva ") + String(curveId) + String(": [") + String(tiempo) + String(",") + String(value) + String("]"));
    return 0; // Éxito
}

//Imprime por consola una curva en particular
//La app no toma como valida una curva enviada por esta funcion
void printCurves() {
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas es NULL"));
        return;
    }
    writeSerialComln(String("curveArraySize: ") + String(curveArraySize));

    for(int i = 0; i < curveArraySize; i++) {
        writeSerialComln(String("==================================="));
        if(curveArray[i] != NULL) {
            writeSerialComln(String("Curva ") + String(i));
            writeSerialComln(String("Pin asociado: ") + String(curveArray[i]->pin));
            writeSerialComln(String("Estado :") + String(curveArray[i]->enabled ? "Habilitada" : "Deshabilitada"));
            writeSerialComln(String("Timestamp: ") + String(curveArray[i]->timestamp));
            writeSerialComln(String("Cantidad de puntos: ") + String(curveArray[i]->contador));
            writeSerialComln(String("Puntos:[Tiempo, Valor]"));

            int lastIndex = curveArray[i]->contador - 1; // Último punto válido
            for(int j = 0; j <= lastIndex; j++) {
                int t = curveArray[i]->point[j].tiempo;
                int v = curveArray[i]->point[j].value;

                // Siempre imprimir el primero y el último
                if(j == 0 || j == lastIndex || (t != 0 || v != 0)) {
                    char buffer[50];
                    sprintf(buffer, ",[%d,%d]", t, v);
                    writeSerialCom(String(buffer));
                }
            }
            writeSerialComln(""); // Nueva línea al final
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

//Habilita una curva para que comience a ejecutarse
bool enableCurve(int curveId) {
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas no inicializado"));
        return false;
    }
    if (curveId < 0 || curveId >= curveArraySize) {
        writeSerialComln(String("Error: Curva no valida"));
        return false;
    }
    if(curveArray[curveId] == NULL) {
        writeSerialComln(String("Error: Curva no existe"));
        return false;
    }

    //Si esta habilitada desahbilita y viceversa

    unsigned long t0 = getCurrentEpoch();


    curveArray[curveId]->enabled = !curveArray[curveId]->enabled;
    if(curveArray[curveId]->enabled==true)
        curveArray[curveId]->timestamp = t0;
    curveArray[curveId]->currentIndex = 0;

    return curveArray[curveId]->enabled;
}



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

