#include <string>
#include <ctype.h>


#include "userInterface.h"
#include <ADCData.h>
#include "modulos/queueCom/queueCom.h"

#include <modulos/carga_electronica/carga_electronica.h>
#include <modulos/time/time.h>
#include <modulos/simuladorCurvas/simuladorCurvas.h>

#define ARRAY_SIZE 3 // Tamaño del arreglo de curvas

#define MAX_DATA_BUFFER 30
#define SEND_DATA true
#define NOT_SEND_DATA false

static MenuNode *menu = nullptr;
String data_buffer = ""; //Variable para almacenar los datos recibidos
bool aceptandoDatos=false;
bool updateScreen=false;

static int lastMenuId = -1;



void moveCursor(int row, int col);
void procesarDatos(String data);
void printSensorData();

bool loadConfiguration();

void printSensor(ADCData data);

bool parseStringToPoint(String str, int* curve, int* tiempo, float* value, aproximation_point_type_t *type);

static void onEnterNode(MenuNode* n);
static void onUpdateNode(MenuNode* n);
static bool nodeRequiresInput(int id);

bool parseStringToInts(String str, int *num1, int *num2);
void printSavedCurves();
void printAllCurves();
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
    char charReceived = readSerialChar();

    // Caso especial: refrescar sensores en pantalla
    if (updateScreen == true) {
        clearScreen();
        printNode(menu);
        printSensorData();
    }

    // Si no hay caracter recibido o menú no inicializado, no hacemos nada
    if (charReceived == '\0' || menu == nullptr) {
        // 🔹 Ejecutar actualizaciones periódicas igual
        onUpdateNode(menu);
        return;
    }

    // Cambiar estado de aceptar datos
    if (charReceived == '-') {
        if (aceptandoDatos == true) {
            aceptandoDatos = false;
            procesarDatos(data_buffer);
            data_buffer = "";
        } else {
            aceptandoDatos = true;
            data_buffer = "";
        }
        return;
    }

    // 🔹 Si NO estamos aceptando datos, procesamos navegación de menú
    if (aceptandoDatos == false) {
        menuUpdate(charReceived, &menu);

        if (menu->id != 1) {
            updateScreen = false;
        }

        clearScreen();
        printNode(menu);

        // Solo disparar acciones si realmente cambió el nodo
        if (lastMenuId != menu->id) {
            lastMenuId = menu->id;
            onEnterNode(menu);   // Ejecutar solo una vez al entrar
        }

        // Manejo especial del menú 1 (sensores) → primer refresh
        if (updateScreen == false) {
            if (menu->id == 1) {
                printSensorData();
                updateScreen = true;
            }
        }

    } else {
        // Estamos recibiendo datos → agregarlos al buffer
        data_buffer += charReceived;
    }

    // 🔹 Ejecutar siempre la lógica de actualización periódica
    onUpdateNode(menu);

    return;
}


bool loadConfiguration(){
    if (readValueNVS("mode") == SEND_DATA) { // Si el modo es SEND_DATA, se activa la opción de enviar datos
        changeMode(SEND_DATA); // Cambiar el modo a SEND_DATA
        writeSerialComln(String("Modo SEND DATA activado"));
        return true;
    } else {
        changeMode(NOT_SEND_DATA); // Cambiar el modo a RECEIVE_DATA
        writeSerialComln(String("Modo SEND DATA desactivado"));
        return false;
    }
}








void procesarDatos(String data) {
    if (data.isEmpty() || menu == nullptr) { // Mejor forma de validar String vacío
        return;
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
    if(menu->id==1){
        writeSerialComln(String("Datos de sensor"));
        printSensorData();
    }
    






    if(menu->id==8){
        float currentReference = data.toFloat(); // Convertir el String a entero
        //ToDo//int current=PWMSetDC(dutyCycle);
        float current=currentReference;
        if (current>=0.0 && current<=1000.0) {
            PWMSetDC(current); // Cambiar el Duty Cycle
            writeSerialComln(String("Corriente de referencia cambiada a: ") + String(current) + "mA");
        } else {
            writeSerialComln(String("Valor de corriente de referencia inválido. Debe estar entre 0 y 1000mA."));
        }
    }
    if(menu->id ==9){
        int frequency = data.toInt(); // Convertir el String a entero
        if (PWMSetFrequency(frequency)==true) {
            writeSerialComln(String("Frecuencia cambiada a: ") + String(frequency) + " Hz");
        } else {
            writeSerialComln(String("Valor de frecuencia inválido. Debe ser mayor que 0."));
        }
    }
    if(menu->id ==10){
        float duty = data.toFloat(); // Convertir el String a entero
        if (PWMSetMaxDC(duty)==true) {
            writeSerialComln(String("Valor maximo de duty cycle: ") + String(duty) + "%");
        } else {
            writeSerialComln(String("Valor máximo de duty cycle inválido. Debe estar entre 0 y 100%."));
        }
    }

    if (menu->id == 11){
        float current = data.toFloat(); 
        if (setMaxCurrent(current)==true) {
            writeSerialComln(String("Valor maximo de corriente: ") + String(current) + "mA");
        } else {
            writeSerialComln(String("Valor máximo de corriente inválido. Debe estar entre 0 y 1000mA."));
        }
    }

    
    if(menu->id ==17){



    }

    if(menu->id==18){

        int num1;
        if(sscanf(data.c_str(), "%d", &num1) == 1){
            int id =createCurve(num1);
            if(id>=0){
                writeSerialComln(String("Curva creada en pin: ") + String(num1) +String(" con id:")+String(id) );
            }else{
                writeSerialComln(String("Error al crear la curva en el pin: ") + String(num1) );

            }
        }else{
            writeSerialComln(String("Error: Formato inválido. Introduze numero de pin"));
        }

    }

    if(menu->id ==19){
        printCargaElectronica();
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
            writeSerialComln(String("❌ Formato inválido. Use [curve,tiempo,value]"));
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
            if(enableCurve(curveId,pin)==true){
                writeSerialComln(String("Curva ") + String(curveId) + String(" habilitada"));
            }else{
                writeSerialComln(String("Curva ") + String(curveId) + String(" deshabilitada"));
            }
        }
        

    }
    if(menu->id ==21){
        if(data.equalsIgnoreCase("y")){
            PWMSetCurveMode(ON_t);
            writeSerialComln(String("Modo curva activado"));
            saveValueNVS(MODO_CURVA, true) ;
        }else if(data.equalsIgnoreCase("n")){
            PWMSetCurveMode(OFF_t);
            writeSerialComln(String("Modo curva desactivado"));
            saveValueNVS(MODO_CURVA, false) ;
        }else{
            writeSerialComln(String("Error: Valor invalido"));
        }

    }
    if(menu->id ==22){
        writeSerialComln(String("Selecciona la curva a guardar (ID):"));
        printCargaElectronica();
        int curveId = data.toInt();
        saveCurveNVS(("curve" + String(curveId)).c_str(), curveId);

    }
    if(menu->id ==23){
        int curveId = data.toInt();

        loadCurveNVS(("curve" + String(curveId)).c_str());
        printCargaElectronica();

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
    if(menu->id ==27){
        if(data.equalsIgnoreCase("PID")){
            changeControlMode(PID);
            writeSerialComln(String("Modo de control cambiado a PID"));
        }else if(data.equalsIgnoreCase("NONE")){
            changeControlMode(NONE);
            writeSerialComln(String("Modo de control cambiado a NONE"));
        }
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
    writeSerialComln(String("Parseando: ") + str);

    // Convertir el String de Arduino a un const char* para sscanf
    if (sscanf(str.c_str(), "%d,%d", num1, num2) == 2) {
        return true;  // Se leyeron correctamente ambos números
    }
    return false;     // No se leyeron correctamente
}
    




void printSensorData() {
    ADCData data;
    //ADCData data = {A0, 5.0, 10.0, 2.5, 12.5, millis()};

    if(receiveSensorDataToUserInterface(&data)==false){
        return;
    }
 
    writeSerialComln(String("Pin: ") + String(data.pin));
    writeSerialComln(String("\tBus Voltage: ") + String(data.busVoltage_V) + String(" V"));
    writeSerialComln(String("\tShunt Voltage: ") + String(data.shuntVoltage_mV) + String(" mV"));
    writeSerialComln(String("\tCurrent: ") + String(data.current_mA) + String(" mA"));
    writeSerialComln(String("\tPower: ") + String(data.power_mW) + String(" mW"));

}



bool parseStringToPoint(String str, int *curve, int *tiempo, float *value, aproximation_point_type_t *type) {
    writeSerialComln(String("Parseando: ") + str);

    int typeInt = 0;


    if (sscanf(str.c_str(), "[%d,%d,%f,%d]", curve, tiempo, value, &typeInt) == 4) {
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
        case 20: // Agregar punto [curva,tiempo,valor]
        case 21: // Activar modo curva (Y/N)
        case 22: // Guardar curva -> requiere ID
        case 23: // Cargar curva -> requiere ID
        case 25: // Modificar fecha
        case 27: // Cambiar modo de control (PID/NONE)
            return true;
        default:
            return false;
    }
}

static void onEnterNode(MenuNode* n) {
    if (!n) return;

    // Acciones inmediatas (sin pedir datos) y automaticas en elupdate
    switch (n->id) {
        case 1:  // Entradas analógicas
            printSensorData();
            updateScreen = true; // ya lo usabas para refrescar periódicamente
            break;
        case 10:
            writeSerialComln(String("Valor maximo actual del PWM: ") + String(PWMGetMaxDC()) + String(" %"));
            writeSerialComln("Ingrese nuevo valor (0..100) y presione '-'");
            break;
        case 15: // Ver curvas
            printCurves();
            break;
        case 16:
            printCurves();
            break;
        case 19: // Seleccionar curva (al menos mostrar algo útil)
            printCargaElectronica();
            break;
        case 23: 
            printAllCurves();
            
            break;
       case 26:
            printPinToCurve();
            break;
        case 27:
            writeSerialComln(String("Modo de control actual: ") + (getModoFuncionamiento() == PID ? "PID" : "NONE"));
            break;
        default:
            break;
    }

    // Nodos que requieren datos: activar captura y mostrar prompt
    if (nodeRequiresInput(n->id)) {
        aceptandoDatos = true;
        data_buffer = "";
        switch (n->id) {
            case 3:  writeSerialComln("Ingrese SSID y presione '-' para confirmar"); break;
            case 4:  writeSerialComln("Ingrese PASSWORD y presione '-'"); break;
            case 6:  writeSerialComln("Para activar/desactivar el modo SEND DATA ingrese y/n y presione '-'"); break;
            case 8:  writeSerialComln("Ingrese la corriente de referencia (0-1000mA) y presione '-'"); break;
            case 9:  writeSerialComln("Ingrese frecuencia <0-78125> y presione '-'"); break;
            case 16: writeSerialComln("ID de curva a habilitar/deshabilitar y pin asociado <ID,pin> luego presione '-'"); break;
            case 18: writeSerialComln("Introduzca el pin asociado a la curva y presione '-'"); break;
            case 20: writeSerialComln("Formato: [curva,tiempo,valor,tipo] y presione '-'"); break;
            case 21: writeSerialComln("Activar modo curva (Y/N) y presione '-'"); break;
            case 22: writeSerialComln("ID de curva a GUARDAR y presione '-'"); break;
            case 23: writeSerialComln("Ingrese el ID de la curva a CARGAR y presione '-'");break;
            case 25: writeSerialComln("Ingrese nueva fecha en formato DD/MM/AAAA HH:MM y presione '-'"); break;
            case 27: writeSerialComln("Ingrese 'PID' o 'NONE' y presione '-'"); break;
            default: break;
        }
    }
}


static void onUpdateNode(MenuNode* n) {
    if (!n) return;

    switch (n->id) {
        case 1: // Menú de sensores
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
//Imprime todas las curvas guardadas en NVS
void printAllCurves() {
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
