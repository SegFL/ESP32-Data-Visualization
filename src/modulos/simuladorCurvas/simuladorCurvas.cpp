

#include "simuladorCurvas.h"
#include <stdio.h>
#include <modulos/serialCom/serialCom.h>

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
    curve->timestamp = millis(); // CORRECCIÓN: Inicializar con tiempo actual
    curve->point[0].tiempo = 0;
    curve->point[0].value = 0;
    curve->contador = 1;
    
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
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas no inicializado"));
        return -1;
    }
    if (curveId < 0 || curveId >= curveArraySize) {
        writeSerialComln(String("Error: Curva no valida"));
        return -1; // Curva no válida
    }
    curve_t *curve = curveArray[curveId];
    // Verifica que curve y curve->point no sean NULL   
    if (!curve || !curve->point || curve->contador < 2) return -1;

    // CORRECCIÓN CRÍTICA: Verificar límites ANTES de cualquier acceso
    if(curve->contador >= curve->size || curve->contador < 0) {
        // Si llegamos al final de la curva, mantener el último valor válido
        if(curve->size > 0) {
            return curve->point[curve->size - 1].value;
        }
        return -1;
    }

    unsigned long currentTime = millis();

    //Me fijo si paso el tiempo suficiente como
    //para avanzar al siguiente punto
    if(currentTime > curve->timestamp) {   
        // CORRECCIÓN: Verificar límites antes de acceder al array
        if(curve->contador < curve->size) {
            int value = curve->point[curve->contador].value;
            curve->contador++;
            return value;
        } else {
            // Ya estamos al final, mantener el último valor
            return curve->point[curve->size - 1].value;
        }
    }else{
        // CORRECCIÓN: Verificar límites antes de acceder
        if(curve->contador < curve->size) {
            return curve->point[curve->contador].value;
        } else {
            return curve->point[curve->size - 1].value;
        }
    }
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
void printCurves(){
    if(curveArray == NULL) {
        writeSerialComln(String("Error: Array de curvas es NULL"));
        return;
    }
    writeSerialComln(String("curveArraySize: ") + String(curveArraySize));

    for(int i=0;i<curveArraySize;i++){
        writeSerialComln(String("==================================="));
        if(curveArray[i]!=NULL){
            writeSerialComln(String("Curva ") + String(i));
            writeSerialComln(String("Pin asociado: ") + String(curveArray[i]->pin));
            writeSerialComln(String("Estado ") + String(curveArray[i]->enabled ? "Habilitada" : "Deshabilitada"));
            writeSerialComln(String("Cantidad de puntos: ") + String(curveArray[i]->contador));           
            writeSerialComln(String("Puntos:[Tiempo, Valor]"));
            for(int j=0;j<curveArray[i]->contador;j++){
                    char buffer[50];
                    sprintf(buffer, ",[%d,%d]", curveArray[i]->point[j].tiempo, curveArray[i]->point[j].value);
                    writeSerialCom(String(buffer));
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