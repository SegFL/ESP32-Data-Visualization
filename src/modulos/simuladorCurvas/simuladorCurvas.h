
// Definición del typedef para un punto con dos enteros
typedef struct {
    int tiempo;
    int value;
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
    int size;
    int pin;
    unsigned long timestamp;
    bool enabled;
} curve_t;



curve_t* createCurve(int pin) ;
curve_t* addPoint(curve_t *curve, int tiempo, int value);
void UpdateCurve(curve_t *curve) ;
curve_t** newCurveArray(int size);
void printCurves(curve_t** curveArray, int size);

