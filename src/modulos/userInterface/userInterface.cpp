#include <string>
#include <ctype.h>


#include "userInterface.h"
#include <ADCData.h>
#include "modulos/queueCom/queueCom.h"

#include <modulos/carga_electronica/carga_electronica.h>
#include <modulos/time/time.h>
#include <modulos/simuladorCurvas/simuladorCurvas.h>
#include <modulos/pid/pid.h>
#include <modulos/estructura_datos/estructura_datos.h>
#include <modulos/dac/dac.h>

#define ARRAY_SIZE 3 // Tamaño del arreglo de curvas

#define MAX_DATA_BUFFER 30
#define SEND_DATA true
#define NOT_SEND_DATA false
#define MAX_SENSORS 3   //Cantidad de sensores a imprimir en el menu se sensores analogicos

#define MAX_APP_BUFFER 128
char app_buffer[MAX_APP_BUFFER];
int app_index = 0;

bool APP_MODE = false;


static MenuNode *menu = nullptr;
char data_buffer[MAX_DATA_BUFFER] ; //Variable para almacenar los datos recibidos
unsigned char buffer_index = 0; //Índice para el buffer de datos
bool aceptandoDatos=false;
bool updateScreen=false;

static int lastMenuId = -1;



void moveCursor(int row, int col);
void procesarDatos(String data);
void printSensorData();
void handleAppMode(char c);
bool loadConfiguration();
bool procesarComandoApp(String cmd);
void printSensor(ADCData data);
void  cleanBufferApp();
bool parseStringToPoint(String str, int* curve, int* tiempo, float* value, aproximation_point_type_t *type);

static void onEnterNode(MenuNode* n);
static void onUpdateNode(MenuNode* n);
static bool nodeRequiresInput(int id);

bool parseStringToInts(String str, int *num1, int *num2);
bool parseStringToFloats(String str, int *index, float *num1, float *num2, float *num3);
void printSavedCurves();
void printSensorInfo();



void userInterfaceInit(){
    serialComInit();
    clearScreen();//Borra mensajes del ESP32 al iniciar el programa
    menu=menuInit();
    if(menu){
        writeSerialComln(String("Menu inicializado"));
    }else{
        writeSerialComln(String("Menu no inicializado"));
    }
    if(loadConfiguration()){
        writeSerialComln(String("Configuracion cargada correctamente"));
    } else{
        writeSerialComln(String("Error al cargar la configuracion"));
    }

    // CORRECCIÓN CRÍTICA: NO inicializar simuladorCurvas aquí
    // Ya se inicializa en CargaElectronicaInit() y causar heap corruption
    // simuladorCurvasInit(ARRAY_SIZE); // DESHABILITADO - CAUSABA HEAP CORRUPTION

}



void userInterfaceUpdate() {
    if (menu == nullptr) return;

    char charReceived = readSerialChar();
    if (charReceived == '@') {
        APP_MODE = true;

        app_index = 0;
        memset(app_buffer, 0, sizeof(app_buffer));
        while(Serial.available()) Serial.read();  // Descarto todos los datos pendientes antes de pasar almodoapp

        writeSerialComlnAPP("APP_MODE_ON");
        return;
    }

    if (charReceived == '#') {
        APP_MODE = false;
        writeSerialComlnAPP("APP_MODE_OFF");
        cleanBufferApp();
        clearScreen();
        printNode(menu);
        onEnterNode(menu);
        return;
    }

    if (APP_MODE) {
        if (charReceived == '\0') return;  // ← AGREGAR ESTO
        handleAppMode(charReceived);
        return;
    }

    if(charReceived== GO_BACK || charReceived == '<'){ // ESCAPE
        charReceived = GO_BACK; // Normalizamos ambos casos a GO_BACK
        menuUpdate(charReceived, &menu);
        
        clearScreen();
        printNode(menu);
        onEnterNode(menu);
        lastMenuId = menu->id;
        
        //Si el nodo es nuevo y requiere datos, preparo el buffer para recibirlos
        //Si no es nuevo pero aun asi requiere datos(porque ya se enviaron datos previamente
        //y se quiere seguir enviando datos) tambien preparo el buffer
        aceptandoDatos = nodeRequiresInput(menu->id);

        
        return ;
    }
  

    if (charReceived == '\n') {
        if (aceptandoDatos) {
            // Terminamos de recibir datos
            data_buffer[buffer_index] = '\0';  // Terminador nulo
            procesarDatos(data_buffer);
            memset(data_buffer, 0, sizeof(data_buffer));
            buffer_index = 0;
            aceptandoDatos = false;
        } 
        return;
    }

   if (!aceptandoDatos) {

        
        menuUpdate(charReceived, &menu);

        if (lastMenuId != menu->id) {
            clearScreen();
            printNode(menu);
            onEnterNode(menu);
            lastMenuId = menu->id;
        }
        //Si el nodo es nuevo y requiere datos, preparo el buffer para recibirlos
        //Si no es nuevo pero aun asi requiere datos(porque ya se enviaron datos previamente
        //y se quiere seguir enviando datos) tambien preparo el buffer
        aceptandoDatos = nodeRequiresInput(menu->id);

    } else {
        // Captura de caracteres
        if(buffer_index < MAX_DATA_BUFFER - 1) {
            // Aceptamos solo números,caracteres , coma y espacios y puntos para los floats
            if (isdigit(charReceived) || isalpha(charReceived) || 
                charReceived == ',' || charReceived == '.' || 
                charReceived == '-' || isspace(charReceived)) {
                data_buffer[buffer_index++] = charReceived;
            }
        }
    }




    // 🔹 Ejecutar siempre la lógica de actualización periódica
    onUpdateNode(menu);

    return;
}


bool loadConfiguration() {

    char mode = 0;

    // Intentar leer el valor desde NVS
    if (!readValueNVS("mode", &mode)) {
        // No existe la clave o error → estado por defecto
        changeMode(NOT_SEND_DATA);
        writeSerialComln("Modo SEND DATA desactivado (valor por defecto)");
        return false;   // falló la carga desde NVS
    }

    // Interpretar el valor leído
    if (mode == SEND_DATA) {
        changeMode(SEND_DATA);
        writeSerialComln("Modo SEND DATA activado");
    } else {
        changeMode(NOT_SEND_DATA);
        writeSerialComln("Modo SEND DATA desactivado");
    }

    return true;    // carga exitosa
}









void procesarDatos(String data) {
        

    if(data.isEmpty()){
        return;
    }
    if (data.isEmpty() || menu == nullptr) { // Mejor forma de validar String vacío
        return;
    }
    

    if(menu->id==1){
        writeSerialComln(String("Datos de sensores"));
        printSensorData();
    }
    

    if (menu->id == 3) {
        setSSID(data); // Cambiar el SSID
        writeSerialComln(String("SSID cambiado a: ") + data);
    }
    if (menu->id == 4) {
        setPassWord(data); // Cambiar el PASSWORD
        writeSerialComln(String("SSID cambiado a *** "));
    }
    if (menu->id == 6) {
        if (data.equalsIgnoreCase("y")) { // Comparación más eficiente
            changeMode(SEND_DATA); // Cambiar el modo a SEND_DATA
            writeSerialComln(String("Modo SEND DATA activado"));
            saveValueNVS("mode", SEND_DATA); // Guardar el modo en NVS
        }
        
        if (data.equalsIgnoreCase("n")) { 
            changeMode(NOT_SEND_DATA); 
            writeSerialComln(String("Modo SEND DATA desactivado"));
            saveValueNVS("mode", NOT_SEND_DATA); 
        }

    }
    

    if (menu->id == 8) {

        int index = -1;
        float current = -1.0f;

        // Parsear "index,corriente"
        if (sscanf(data.c_str(), "%d,%f", &index, &current) == 2) {

            // Validaciones
            if (index < 0 ) {
                writeSerialComln("Indice de curva invalido");
                return;
            }

            if (current < 0.0f || current > 6000.0f) {
                writeSerialComln("Corriente invalida (0..6000 mA)");
                return;
            }

            // OK
            if(setCurrentReference_mA(current, index)){
                getCurrentReference_mA(index); // Para imprimir el valor actualizado en consola
                writeSerialComln(
                    String("Corriente de referencia cambiada: Curva ")
                    + String(index)
                    + String(" -> ")
                    + String(current)
                    + String(" mA")
                );
            } else {
                writeSerialComln("Error al cambiar la corriente de referencia");
            }


        } else {
            writeSerialComln(
                "Formato invalido. Use: <indice>,<corriente_mA>  (ej: 0,500)"
            );
        }
    }
    
    if (menu->id == 9) {

        int index = -1;
        int frequency = 0;

        // Formato esperado: index,frecuencia
        if (sscanf(data.c_str(), "%d,%d", &index, &frequency) != 2) {
            writeSerialComln("Error: formato invalido. Use index,frecuencia");
            return;
        }
        // Validar frecuencia
        if (frequency <= 0) {
            writeSerialComln("Error: la frecuencia debe ser mayor que 0");
            return;
        }

        // Aplicar cambio
        if (PWMSetFrequency(frequency,index)) {
            writeSerialComln(
                String("PWM ") + index +
                String(" -> Frecuencia cambiada a: ") +
                String(frequency) + " Hz"
            );
        } else {
            writeSerialComln("Error al configurar la frecuencia PWM");
        }
    }
    

    if (menu->id == 10) {

        int index = -1;
        float duty = -1.0f;

        // Espera formato: index,valor
        if (sscanf(data.c_str(), "%d,%f", &index, &duty) != 2) {
            writeSerialComln("Error: formato invalido. Use index,duty (ej: 0,75.5)");
            return;
        }

        // Validar duty
        if (duty < 0.0f || duty > 100.0f) {
            writeSerialComln("Error: duty invalido (0..100%)");
            return;
        }

        // Aplicar
        if (PWMSetMaxDC( duty,index)) {
            float maxDC = PWMGetMaxDC(index); // Para imprimir el valor actualizado en consola
            writeSerialComln(
                String("Curva ") + String(index) +
                String(" -> Max duty cycle: ") + String(maxDC) + "%"
            );
        } else {
            writeSerialComln("Error al configurar max duty cycle");
        }
    }

    



    
    if(menu->id ==17){



    }

    
    if(menu->id==18){

        int num1;
        writeSerialComln(String("Creando curva para el pin: ") + data);
        if(sscanf(data.c_str(), "%d", &num1) == 1){
            int id =createCurve(num1);
            if(id>0){
                writeSerialComln(String("Curva creada con id: ") + String(id) );
            }else{
                writeSerialComln(String("Error al crear la curva en el pin: ") + String(num1) );

            }
        }else{
            writeSerialComln(String("Error: Formato inválido. Introduze numero de pin"));
        }

    }

if(menu->id == 19) {
    // Debug: imprimir el string raw caracter por caracter

    int curveId;
    float Imin, Imax, Vmin, Vmax, Pmin, Pmax;
    
    int parsed = sscanf(data.c_str(), "%d,%f,%f,%f,%f,%f,%f", 
                        &curveId, &Imin, &Imax, &Vmin, &Vmax, &Pmin, &Pmax);

    writeSerialComln(String("Parsed: ") + String(parsed));
    writeSerialComln(String("CurveId=") + String(curveId) + 
                     String(" Imin=") + String(Imin) + 
                     String(" Imax=") + String(Imax) +
                     String(" Vmin=") + String(Vmin) + 
                     String(" Vmax=") + String(Vmax) +
                     String(" Pmin=") + String(Pmin) + 
                     String(" Pmax=") + String(Pmax));

    if(parsed == 7) {
        if(!setLimits(curveId, Imin, Imax, Vmin, Vmax, Pmin, Pmax)) {
            writeSerialComln(String("Error al setear limites en curva: ") + String(curveId));
        }
    } else {
        writeSerialComln(String("Error: Formato invalido. Introduzca: CurveId,Imin,Imax,Vmin,Vmax,Pmin,Pmax"));
        writeSerialComln(String("Ejemplo: 1,-1,10.0,-1,30.0,-1,100.0 (-1 para ignorar un limite)"));
    }
}

    if(menu->id ==20){
        int curve, tiempo;
        float value;
        aproximation_point_type_t type=STEP; //Por defecto es STEP
        if (parseStringToPoint(data.c_str(), &curve, &tiempo, &value,&type)) {
            if (addPointToCurve(curve, tiempo, value,type) == 0) {
                writeSerialComln(String("Punto agregado a la curva ") + String(curve) + String(": [Tiempo: ") + String(tiempo) + String(", Valor: ") + String(value) + String(", Tipo: ") + (type == LINEAR ? "LINEAR" : (type == STEP ? "STEP" : "S_CURVE")) + String("]"));
            }
        } else {
            writeSerialComln(String("Formato inválido. Use [curve,tiempo,value]"));
        }
    }
    
    if(menu)

    if(menu->id ==15){
        int curveId = data.toInt();
        writeSerialComln(String("Curvas actuales:"));
        printCargaElectronica();
    }

    if(menu->id ==16){

        int curveId = -1;
        int pin = -1;

        if (sscanf(data.c_str(), "%d,%d", &curveId, &pin) == 2){
            if(asociarCurvaAPin(curveId,pin)==true){
                writeSerialComln(String("Curva ") + String(curveId) + String(" asociada al pin: ") + String(pin));
            }else{
                writeSerialComln(String("No se pudo ascociar la curva ") + String(curveId) + String(" al pin: ") + String(pin));
            }
        }
        

    }
    if (menu->id == 21) {

        int index = -1;
        if (sscanf(data.c_str(), "%d", &index) != 1) {
            writeSerialComln("Error: debe ingresar un numero de curva");
            return;
        }
        // Toggle
        if(getCurveMode(index)==ON_t){
            PWMSetCurveMode(OFF_t, index);
        }else{
            PWMSetCurveMode(ON_t, index);
        }
        writeSerialComln(String("Modo de curva para indice ") + String(index) + String(" cambiado a ") + (getCurveMode(index) == ON_t ? "ON" : "OFF"));
    }

    if(menu->id ==22){
        int curveId = data.toInt();
        saveCurveNVS(("curve" + String(curveId)).c_str(), curveId);

    }
    if(menu->id ==23){
        int curveId = data.toInt();

        loadCurveNVS(("curve" + String(curveId)).c_str());

    }

    if(menu->id ==25){
        // Formato esperado: "DD/MM/AAAA HH:MM"
        int day, month, year, hour, minute;
        if (sscanf(data.c_str(), "%d/%d/%d %d:%d", &day, &month, &year, &hour, &minute) == 5) {
            if (setDateTime(day, month, year, hour, minute)) {
                writeSerialComln(String("Fecha y hora actualizadas a: ") + data);
            } else {
                writeSerialComln(String("Error: Fecha u hora inválida."));
            }
        } else {
            writeSerialComln(String("Error: Formato inválido. Use DD/MM/AAAA HH:MM"));
        }
    }
    if (menu->id == 30) {

        int index = -1;
        char modeStr[8];   // suficiente para "PID" o "NONE"


        if (sscanf(data.c_str(), "%d,%7s", &index, modeStr) != 2) {
            writeSerialComln("Error: formato invalido. Use index,PID o index,NONE");
            return;
        }
        // Interpretar modo
        if (strcasecmp(modeStr, "PID") == 0) {
            if(setControlMode(PID,index)==false){
                writeSerialComln(String("Error al cambiar el modo de control. Index invalido: ") + String(index));
                return;
            }
            writeSerialComln(String("Curva ") + index + " -> Modo de control PID");
        }
        else if (strcasecmp(modeStr, "NONE") == 0) {
            if(setControlMode(NONE,index)==false){
                writeSerialComln(String("Error al cambiar el modo de control. Index invalido: ") + String(index));
                return;
            }
            writeSerialComln(String("Curva ") + index + " -> Modo de control NONE");
        }else {
            writeSerialComln("Error: modo invalido ( < index > , < PID / NONE> )");
        }
    }

    if(menu->id ==28){
        
        int curveId = data.toInt();
        writeSerialComln("Eliminando curva"+String(curveId));
        if(deleteCurve(curveId)==true){
            writeSerialComln(String("Curva ") + String(curveId) + String(" eliminada correctamente"));
        }else{
            writeSerialComln(String("Error al eliminar la curva ") + String(curveId));
        }
    }
    if(menu->id ==29){
        int curveId = data.toInt();
        if(( deleteCurveNVS(("curve" + String(curveId)).c_str()))){
            writeSerialComln(String("Curva ") + String(curveId) + String(" eliminada correctamente de la flash"));
        }else{
            writeSerialComln(String("Error al eliminar la curva de la flash") + String(curveId));
        }
    }
    if (menu->id == 31) {

        int index = -1;

        // Verificar que data sea un número entero > 0
        if (sscanf(data.c_str(), "%d", &index) == 1 && index > 0) {

            if(resetPID(index)!=true){
                writeSerialComln(
                    String("Error al resetear el PID. Index invalido: ") + String(index)
                );
                return;
            }
            writeSerialComln(
                String("PID reseteado correctamente. Index: ") + String(index)
            );

        } else {
            writeSerialComln(
                "Valor invalido. Ingrese un numero mayor a 0"
            );
        }
    }

    if(menu->id == 32){
        int index = -1;
        float kp, ki, kd;
        if (parseStringToFloats(data,&index, &kp, &ki, &kd)) {
            if(index < 0){
                writeSerialComln("Index inválido. Debe ser mayor o igual a 0.");
                return;
            }
            if(kp < 0 || ki < 0 || kd < 0){
                writeSerialComln("Parámetros PID inválidos. Kp, Ki y Kd deben ser mayores o iguales a 0.");
                return;
            }
            setPIDParams(index,kp, ki, kd);
            writeSerialComln("Parámetros PID actualizados:");
            writeSerialComln(String("Index: ") + String(index));
            writeSerialComln(String("Kp: ") + String(kp, 3));
            writeSerialComln(String("Ki: ") + String(ki, 3));
            writeSerialComln(String("Kd: ") + String(kd, 3));
        } else {
            writeSerialComln("Formato inválido. Use: Kp,Ki,Kd (ejemplo: 1.5,0.2,0.1)");
        }
    }
    if(menu->id ==33){
            if(getWiFiStatus()==true){
                writeSerialComln(String("Estado de WiFi: Conectado"));
            }else{
                writeSerialComln(String("Estado de WiFi: Desconectado"));
            }
        
    }

    if (menu->id == 34) {
        int index = -1;
        int value = -1;

        if (sscanf(data.c_str(), "%d,%d", &index, &value) != 2) {
            writeSerialComln("Error: formato invalido. Use <index>,<0/1>");
            return;
        }

        if (value != 0 && value != 1) {
            writeSerialComln("Error: valor invalido. Use 0 (deshabilitar) o 1 (habilitar)");
            return;
        }

        if (!setFeedforwardEnabled((bool)value, index)) {
            writeSerialComln(String("Error al modificar feedforward. Index invalido: ") + String(index));
            return;
        }

        bool enabled = false;
        getFeedforwardEnabled(&enabled, index);
        writeSerialComln(
            String("Curva ") + String(index) +
            String(": Feedforward ") +
            (enabled ? "HABILITADO" : "DESHABILITADO")
        );
    }








    
}





void printSensor(ADCData data){
    writeSerialComln(String("Pin: ") + String(data.pin));
    writeSerialComln(String("\tBus Voltage: ") + String(data.busVoltage_V) + String(" V"));
    writeSerialComln(String("\tShunt Voltage: ") + String(data.shuntVoltage_mV) + String(" mV"));
    writeSerialComln(String("\tCurrent: ") + String(data.current_mA) + String(" mA"));
    writeSerialComln(String("\tPower: ") + String(data.power_mW) + String(" mW"));
    //writeSerialComln(String("\tTiempo: ") + getFormattedDateTime());
}

bool parseStringToInts(String str, int *num1, int *num2) {

    // Convertir el String de Arduino a un const char* para sscanf
    if (sscanf(str.c_str(), "%d,%d", num1, num2) == 2) {
        return true;  // Se leyeron correctamente ambos números
    }
    return false;     // No se leyeron correctamente
}

bool parseStringToFloats(String str, int *index, float *num1, float *num2, float *num3) {

    // Convertir el String de Arduino a un const char* para sscanf
    if (sscanf(str.c_str(), "%d,%f,%f,%f", index, num1, num2, num3) == 4) {
        return true;  // Se leyeron correctamente los tres números
    }
    return false;     // No se leyeron correctamente
}
    



void printSensorInfo(){

    moveCursor(2,1);
    writeSerialComln(String("Pin: ") );
    writeSerialComln(String("\tBus Voltage: ") + String("    ") + String(" V"));
    writeSerialComln(String("\tShunt Voltage: ") + String("      ") + String(" mV"));
    writeSerialComln(String("\tCurrent: ") + String("      ") + String(" mA"));
    writeSerialComln(String("\tPower: ") + String("      ") + String(" mW"));

}

void printSensorData() {

    // 1) Crear array para recibir TODOS los sensores
    //Se queda con el ultimo valor de cada uno
    ADCData data[NUMBER_OF_SENSORS];

    if(SEND_DATA == false){
        writeSerialComln("Modo SEND DATA desactivado. No se enviarán datos de sensores.");
        return;
    }
    // 2) Llenarlo con receiveSensorDataToUserInterface()
    if (!receiveSensorDataToUserInterface(data)) {
        return;
    }

    // 3) Tamaño en líneas de cada bloque de sensor
    const int LINES_PER_SENSOR = 6;

    // 4) Recorrer todos los sensores por pin
    for (int pin = 0; pin < NUMBER_OF_SENSORS; pin++) {

        // Si no hay datos válidos para este sensor, saltearlo
        // Podés agregar un flag de validez si querés
        // Por ahora asumimos que siempre llegará algún dato
        ADCData sensor = data[pin];

        int baseRow = 2 + pin * LINES_PER_SENSOR;

        // ---- Imprimir bloque ----

        moveCursor(baseRow + 0, 1);
        writeSerialCom(String("Pin ") + String(sensor.pin) + "              ");

        moveCursor(baseRow + 1, 8);
        writeSerialCom("Bus Voltage: " + String(sensor.busVoltage_V) + " V     ");

        moveCursor(baseRow + 2, 8);
        writeSerialCom("Shunt Voltage: " + String(sensor.shuntVoltage_mV) + " mV   ");

        moveCursor(baseRow + 3, 8);
        writeSerialCom("Current: " + String(sensor.current_mA) + " mA     ");

        moveCursor(baseRow + 4, 8);
        writeSerialCom("Power: " + String(sensor.power_mW) + " mW     ");
    }
}



bool parseStringToPoint(String str, int *curve, int *tiempo, float *value, aproximation_point_type_t *type) {

    int typeInt = 0;


    if (sscanf(str.c_str(), "%d,%d,%f,%d", curve, tiempo, value, &typeInt) == 4) {
        switch (typeInt) {
            case 0: *type = STEP; break;
            case 1: *type = LINEAR; break;
            case 2: *type = S_CURVE; break;
            default: return false;
        }
        return true;
    }
    return false;
}






static bool nodeRequiresInput(int id) {
    switch (id) {
        case 3:  // Entre SSID
        case 4:  // Entre contraseña
        case 6:  // Activar/desactivar SEND DATA (y/n)
        case 8:  // Duty cycle
        case 9:  // Frecuencia
        case 10: // Max current reference
        case 16: // Activar curva -> requiere ID
        case 18: // Crear curva -> requiere pin
        case 19: // Modificar limites -> requiere CurveId,Imin,Imax,Vmin,Vmax,Pmin,Pmax
        case 20: // Agregar punto [curva,tiempo,valor]
        case 21: // Activar modo 
        case 22: // Guardar curva -> requiere ID
        case 23: // Cargar curva -> requiere ID
        case 25: // Modificar fecha
        case 28: // Eliminar curva de RAM 
        case 29: // Eliminar curva de nvs
        case 30: // Cambiar modo de control (PID/NONE)
        case 31: // Resetear parámetros PID
        case 32: // Modificar parámetros PID
        case 34: //
            return true;
        default:
            return false;
    }
}

static void onEnterNode(MenuNode* n) {
    if (!n) return;

    if (nodeRequiresInput(n->id)) {
        aceptandoDatos = false;
        memset(data_buffer, 0, sizeof(data_buffer));
        buffer_index = 0;
        //sendUartDataln("Nodo requiere entrada. Presiona 'ENTER' para comenzar.");


    } 

    // Acciones inmediatas (sin pedir datos) y automaticas en elupdate
    switch (n->id) {
        case 1:  // Entradas analógicas
            printSensorInfo();
            //updateScreen = true; // ya lo usabas para refrescar periódicamente
            break;
        case 8:
            for(int i=0;i<NUMBER_OF_ELECTRONIC_LOADS;i++){
                float currentRef = getCurrentReference_mA(i);
                writeSerialComln(String("Curva ") + String(i) + String(": Corriente de referencia actual: ") + String(currentRef) + String(" mA"));
            }
            break;
        case 9:  // Frecuencia PWM
            for (char i = 0; i < 2; i++) {
                int ch, f, res;
                if (getPWMConfig(i, &ch, &f, &res)) {
                    writeSerialComln(
                        String("Canal: ") + ch +
                        "  Frecuencia: " + f + " Hz" +
                        "  Resolucion: " + res + " bits"
                    );
                }
            }

            break;
        case 10:
            for(int i=0;i<getCurveArraySize();i++){
                writeSerialComln(String("Curva ") + String(i) + String(" Valor maximo actual del PWM: ") + String(PWMGetMaxDC(i)) + String(" %"));
            }
            writeSerialComln("Ingrese nuevo valor (0..100) y presione 'ENTER'");
            break;
        case 15: // Ver curvas
            writeSerialComln("Curvas guardadas en RAM");
            printCurves();
            break;
        case 16:
            printCurves();
            break;
        case 19: 
            printAllLimits();
            break;
        case 21: //Imprimir el modo de las curvas
            for(int i=0;i<getCurveArraySize();i++){
                writeSerialComln(String("Curva ") + String(i) + String(": Modo ") + (getCurveMode(i)==ON_t ? "CURVA" : "MANUAL"));
            }
            break;
        case 22:
            writeSerialComln(String("Selecciona la curva a guardar (ID):"));
            printCargaElectronica();
            break;
        case 23: 
            printAllCurvesNvs();
            
            break;
       case 26:
            printPinToCurve();
            break;
        case 30:
            for(int i=0;i<getCurveArraySize();i++){
                writeSerialComln(String("Modo de control actual: ") + (getModoFuncionamiento(i) == PID ? "PID" : "NONE"));
            }
            break;
        case 28:
            printCurves();
            break;
        case 29:
            printAllCurvesNvs();
            break;
        case 31: // Resetear parámetros PID
            {
                for(int i=0;i<2;i++){
                    float kp, ki, kd;
                    getPIDParams(i, &kp, &ki, &kd);
                    writeSerialComln(String("Parámetros PID actuales del index ") + String(i) + String(":"));
                    writeSerialComln(String("Kp: ") + String(kp, 3));
                    writeSerialComln(String("Ki: ") + String(ki, 3));
                    writeSerialComln(String("Kd: ") + String(kd, 3));
                }
            }
            break;
        case 32: // Modificar parámetros PID
            {
                for(int i=0;i<NUMBER_OF_SENSORS;i++){
                    float kp, ki, kd;
                    getPIDParams(i, &kp, &ki, &kd);
                    writeSerialComln(String("Parámetros PID actuales del index ") + String(i) + String(":"));
                    writeSerialComln(String("Kp: ") + String(kp, 3));
                    writeSerialComln(String("Ki: ") + String(ki, 3));
                    writeSerialComln(String("Kd: ") + String(kd, 3));
                }
            }
            break;
        case 33:
        {
            //Intenta realizar una conexion a wifi sin esperar el tiempo de espera
            connectWiFi();
        }

        case 34: // Feedforward
        {
            for (int i = 0; i < NUMBER_OF_ELECTRONIC_LOADS; i++) {
                bool enabled = false;
                getFeedforwardEnabled(&enabled, i);
                writeSerialComln(
                    String("Curva ") + String(i) +
                    String(": Feedforward ") +
                    (enabled ? "HABILITADO" : "DESHABILITADO")
                );
            }
        }

            
        default:
            break;
    }

    // Nodos que requieren datos: activar captura y mostrar prompt
    if (nodeRequiresInput(n->id)) {
        aceptandoDatos = true;
        memset(data_buffer, 0, sizeof(data_buffer));
        buffer_index = 0;

        switch (n->id) {
            case 3:  writeSerialComln("Ingrese SSID y presione 'ENTER' para confirmar"); break;
            case 4:  writeSerialComln("Ingrese PASSWORD y presione 'ENTER'"); break;
            case 6:  writeSerialComln("Para activar/desactivar el modo SEND DATA ingrese y/n y presione 'ENTER'"); break;
            case 8:  writeSerialComln("Ingrese la corriente de referencia <indice>,<corriente_mA>  (ej: 0,500) y presione 'ENTER'"); break;
            case 9:  writeSerialComln("Ingrese frecuencia <0-78125> y presione 'ENTER'"); break;
            case 16: writeSerialComln("ID de curva a habilitar/deshabilitar y pin asociado <ID,pin> luego presione 'ENTER'"); break;
            case 18: writeSerialComln("Introduzca el id deseado de la curva y presione 'ENTER'"); break;
            case 19: writeSerialComln("Ingrese los limites en formato: CurveId,Imin,Imax,Vmin,Vmax,Pmin,Pmax y presione 'ENTER'"); break;
            case 20: writeSerialComln("Formato: [curva,tiempo,valor,tipo] y presione 'ENTER'"); break;
            case 21: writeSerialComln("Presione el numero de curva para cambiar de modo CURVA / MANUAL y luego presione 'ENTER'"); break;
            case 22: writeSerialComln("ID de curva a GUARDAR y presione 'ENTER'"); break;
            case 23: writeSerialComln("Ingrese el ID de la curva a CARGAR y presione 'ENTER'");break;
            case 25: writeSerialComln("Ingrese nueva fecha en formato DD/MM/AAAA HH:MM y presione 'ENTER'"); break;
            case 30: writeSerialComln("Ingrese  < index > , < PID / NONE>  y presione 'ENTER'"); break;
            case 28: writeSerialComln("Ingrese el ID de la curva a eliminar y presione 'ENTER'"); break;
            case 29: writeSerialComln("Ingrese el ID de la curva a eliminar de la flash y presione 'ENTER");break;
            case 31: writeSerialComln("Para resetear los parametros del PID presione y-"); break;
            case 32: writeSerialComln("Ingrese parámetros PID en formato index,Kp,Ki,Kd y presione 'ENTER'"); break;
            case 34: writeSerialComln("Ingrese <index>,<0/1> para deshabilitar/habilitar feedforward y presione 'ENTER'"); break;
            default: break;
        }
    }
}

//Ejecuto acciones periódicas al estar en ciertos nodos
static void onUpdateNode(MenuNode* n) {
    if (!n) return;

    switch (n->id) {
        case 1: // Menú de sensores
            //clearScreen();
            //printNode(n);
            //printSensorInfo();
            printSensorData(); // refrescar siempre
            break;
        case 24: // Ver fecha
            clearScreen();
            printNode(n);
            writeSerialComln(String("Fecha actual: ") + getFormattedDateTime());
            break;

        default:
            // Otros menús no se refrescan constantemente
            break;
    }
}

void printSavedCurves(){

    /*
    bool aux= true;
    char i=0;
    while(aux){
        aux=loadCurveNVS(("curve" + String(i++)).c_str());
    }
    */

}

/*
//Imprime todas las curvas guardadas en NVS
void printAllCurvesNvs() {
    int id = 0;
    while (true) {
        String key = String("curve") + String(id);
        writeSerialComln(String(key));
        if (printCurveFromNvs(key.c_str())==false) {
            break; // cuando ya no existe más, corta
        }
        id++;
    }
}
*/

void handleAppMode(char c) {

    
    if (c == '\r') return;

    if (c == '\n') {
        app_buffer[app_index] = '\0';

        if (app_index == 0) return;  // ignorar líneas vacías
        writeSerialComln(String("APP_MODE cadena recivida:") + String(" (") + String(app_buffer) + String(")"));
        bool ok = procesarComandoApp(String(app_buffer));
        if (!ok) {
            
            writeSerialComlnAPP("ERROR");
        }
        memset(app_buffer, 0, sizeof(app_buffer));
        app_index = 0;
        return;
    }

    if (app_index < MAX_APP_BUFFER - 1) {
        app_buffer[app_index++] = c;
    }
}

bool procesarComandoApp(String cmd)
{
    cmd.trim();

    //writeSerialComlnAPP(String("CMD:[") + cmd + String("]"));
    // ==============================
    // Detecta el comando batch
    if (cmd.startsWith("CURVE,")) {
        // Parsear: CURVE,id,n,t1,v1,tipo1;t2,v2,tipo2;...,checksum
        int lastComma = cmd.lastIndexOf(',');
        String checksumStr = cmd.substring(lastComma + 1);
        String payload     = cmd.substring(0, lastComma);
        
        // Verificar checksum
        uint8_t cs = 0;
        for (int i = 0; i < payload.length(); i++) cs ^= payload[i];
        if (strtoul(checksumStr.c_str(), nullptr, 16) != cs) {
            writeSerialComlnAPP("ERROR,CHECKSUM");
            return false;
        }
        
        // Extraer id y n
        // payload = "CURVE,id,n,puntos"
        int c1 = payload.indexOf(',');
        int c2 = payload.indexOf(',', c1+1);
        int c3 = payload.indexOf(',', c2+1);
        int id = payload.substring(c1+1, c2).toInt();
        int n  = payload.substring(c2+1, c3).toInt();
        
        // Crear la curva
        int idElejido = createCurve(id);
        if (idElejido   < 0) {
            writeSerialComlnAPP("ERROR,CREATE"); 
            return false; 
        }
        //Para limitar posibles errores nole permito al usuario elegir un id diferente al que se le asigno a la curva
        if(idElejido != id){
            if(deleteCurve(idElejido) != 0){
                writeSerialComlnAPP("ERROR,DELETE(id ocupado:" + String(id) + ")");
                return false;
            }
            writeSerialComlnAPP("ERROR,ID_ASIGNADO_" + String(idElejido));
            return false;
        }

        
        // Parsear puntos separados por ';'
        String puntos = payload.substring(c3+1);
        int parsed = 0;
        while (puntos.length() > 0 && parsed < n) {
            int sep = puntos.indexOf(';');
            String pt = (sep >= 0) ? puntos.substring(0, sep) : puntos;
            puntos   = (sep >= 0) ? puntos.substring(sep+1) : "";
            
            int a = pt.indexOf(','), b = pt.lastIndexOf(',');
            int t    = pt.substring(0, a).toInt();
            float v  = pt.substring(a+1, b).toFloat();
            int tipo = pt.substring(b+1).toInt();
            
            aproximation_point_type_t type = (tipo == 1) ? LINEAR : STEP;
            if (addPointToCurve(id, t, v, type) != 0) {
                writeSerialComlnAPP("ERROR,POINT_" + String(parsed));
                return false;
            }
            parsed++;
        }
        
        writeSerialComlnAPP("OK," + String(id));
        return true;
    }
    writeSerialComlnAPP(String(cmd)+" no reconocido");
    return false;
}

void cleanBufferApp(){
   memset(app_buffer, 0, sizeof(app_buffer));
    app_index = 0;
}