

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
curve_t* createCurve(int pin) {
    curve_t *curve = (curve_t *)malloc(sizeof(curve_t));
    if (curve == NULL) {
        // Manejar error: No se pudo asignar memoria
        return NULL;
    }

    curve->point = (point_t *)malloc( sizeof(point_t)*10);
    if (curve->point == NULL) {
        // Manejar error: No se pudo asignar memoria
        free(curve);
        return NULL;
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
    curve->timestamp = 0;
    curve->point[0].tiempo = 0;
    curve->point[0].value = 0;
    curve->contador = 1;

    return curve;
}
void UpdateCurve(curve_t *curve) {

}

curve_t* addPoint(curve_t *curve, int tiempo, int value) {
    if (curve->contador >= curve->size) {
        curve->point=(point_t*)realloc(curve->point, sizeof(point_t) * (curve->size + 10));
        if (curve->point == NULL) {
            // Manejar error: No se pudo asignar memoria
            return NULL;
        }
        return NULL;        
    }


    if(value > curve->Imax){
        (curve->point[curve->contador]).value = curve->Imax;
    }else if(value < curve->Imin){
        (curve->point[curve->contador]).value = curve->Imin;
    }else{
        (curve->point[curve->contador]).value = value;
    }
    //Si el nuevo tiempo es menor que el del punto anterior es invalido
    if(tiempo<=curve->point[curve->contador-1].tiempo){
        writeSerialComln("Error: Tiempo no valido");
        (curve->point[curve->contador]).value = 0;
        return NULL;
    }

    curve->point[curve->contador].tiempo = tiempo;
    curve->point[curve->contador].value = value;
    curve->contador++;

    return curve;
}

void printCurves(curve_t** curveArray, int size){
    for(int i=0;i<size;i++){
        if(curveArray[i]!=NULL){
            writeSerialComln("Curva "+String(i));
            writeSerialComln("Pin: "+String(curveArray[i]->pin));
            writeSerialComln("Cantidad de puntos: "+String(curveArray[i]->contador));           
            writeSerialComln("Puntos:[Tiempo, Valor]");
            for(int j=0;j<curveArray[i]->contador;j++){
                writeSerialComln(String('\t')+String('[')+String(curveArray[i]->point[j].tiempo)+String(',')+String(curveArray[i]->point[j].value+String(']')));
            }
        }
    }
}