#ifndef SIMULADORCURVAS_H
#define SIMULADORCURVAS_H
#include <stdint.h>
#include "../../modulos/time/time.h"

#include <stdio.h>
#include <modulos/serialCom/serialCom.h>
#include "nvs.h"

typedef enum {
    STEP,
    LINEAR,
    S_CURVE //Sin implementar


}aproximation_point_type_t;

// Definición del typedef para un punto con dos enteros
typedef struct {
    int tiempo;
    float value;
    aproximation_point_type_t type; 
} point_t;

// Definición del typedef para una estructura que contiene un puntero a point_t 
// y seis variables enteras
typedef struct {
    point_t *point;   // Puntero al tipo point_t
    int Imax;
    int Imin;
    int Vmax;
    int Vmin;
    int Pmax;
    int Pmin;
    int contador;
    int currentIndex;  
    int size;
    int pin;
    uint32_t timestamp;
    bool enabled;
} curve_t;



int createCurve(int pin);
int addPointToCurve(int curveId, int tiempo, float value,aproximation_point_type_t type);
void UpdateCurve(curve_t *curve) ;
curve_t** newCurveArray(int size);
void printCurves();
int getCurveValue(int curveId);
bool validateCurve(curve_t *curve); // NUEVA: Función de validación
void sendCurves();
void simuladorCurvasInit(int size);
bool deleteCurve(int curveId); // NUEVA: Función para eliminar curva
int getCurveCount(); // NUEVA: Función para obtener cantidad de curvas
bool enableCurve(int curveId);
void saveCurveNVS(const char* key, int curveId);
bool loadCurveNVS(const char* key);

#endif // SIMULADORCURVAS_H