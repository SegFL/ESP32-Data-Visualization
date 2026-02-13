#ifndef SIMULADORCURVAS_H
#define SIMULADORCURVAS_H
#include <stdint.h>
#include "../../modulos/time/time.h"

#include <stdio.h>
#include <modulos/serialCom/serialCom.h>
#include "nvs.h"
#include "../../config.h"
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

typedef struct {
 int index_pasos,numero_pasos=0; //Contador de pasos dentro del segmento
 float delta_v,delta_t,pendiente,incremento=0.0f;
} linear_parameters_t;
// Definición del typedef para una estructura que contiene un puntero a point_t 
// y seis variables enteras
typedef struct {
    int id;
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
    linear_parameters_t linear_parameters;//Parametros
} curve_t;



int createCurve(int pin);
int addPointToCurve(int curveId, int tiempo, float value,aproximation_point_type_t type);
void UpdateCurve(curve_t *curve) ;
curve_t** newCurveArray(int size);
void printCurves();
float getCurveValue(int curveId);
bool validateCurve(curve_t *curve); // NUEVA: Función de validación
void sendCurves();
void simuladorCurvasInit(int size);
bool deleteCurve(int curveId); // NUEVA: Función para eliminar curva
int getCurveCount(); // NUEVA: Función para obtener cantidad de curvas
bool enableCurve(int curveId, int pin);
void saveCurveNVS(const char* key, int curveId);
bool loadCurveNVS(const char* key);
bool deleteCurveNVS(const char* key);
void printCurve(curve_t* c) ;
bool printCurveFromNvs(const char* key);
void printPinToCurve();
bool asociarCurvaAPin(int curveId, int pin);
int getCurveArraySize();
int loadIDsavedNVS(void);

void printAllCurvesNvs();
#endif // SIMULADORCURVAS_H