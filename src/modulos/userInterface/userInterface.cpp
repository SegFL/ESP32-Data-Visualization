#include <nvs.h>
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



void moveCursor(int row, int col);
void procesarDatos(String data);
void printSensorData();

bool loadConfiguration();
void saveValueNVS(const char* key, bool value);
bool readValueNVS(const char* key);
void printSensor(ADCData data);
bool parseStringToInts(String str, int *num1, int *num2);

bool parseStringToInts(const char* str, int* curve, int* tiempo, int* value);

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




void userInterfaceUpdate(){

    char charReceived = readSerialChar();

    if(updateScreen==true){//Caso especial pq se actualizan los datos de los sensores
        clearScreen();
        printNode(menu);
        printSensorData();
    }
    if(charReceived=='\0'||menu==nullptr){
        return;
    }

    if(charReceived=='-'){//Si se presiona enter se cambia el estado de recvir datos
        if(aceptandoDatos==true){
            aceptandoDatos=false;
            //Loopback
            //writeSerialComln("Datos recibidos");
            //writeSerialComln(String(data_buffer));
            procesarDatos(data_buffer);

            data_buffer="";
        }else{
            aceptandoDatos=true;
            data_buffer="";
        }
        return;
    }


    if(aceptandoDatos==false){

        menuUpdate(charReceived, &menu);
        if(menu->id!=1){
            updateScreen=false;
        }
        clearScreen();
        printNode(menu);

        //En el caso que se cambie al estado de menu 1 se ejecuta este if una sola vez(solo cuando se cambia de estado del menu),
        //el resto de las veces lo hago automaticamente
        if(updateScreen==false){
            if(menu->id==1){
                printSensorData();
                updateScreen=true;
            }
        }




    }else{
        //writeSerialComln("Guarde:"+String(charReceived));
        data_buffer += charReceived;  // Agregarlo al buffer
    }


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


void saveValueNVS(const char* key, bool value) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(my_handle, key, value ? 1 : 0); // Guardar el valor como uint8_t
        if (err == ESP_OK) {
            err = nvs_commit(my_handle); // Confirmar los cambios
        }
        nvs_close(my_handle); // Cerrar el handle
    } else {
        writeSerialComln(String("Error al abrir NVS"));
    }
}

bool readValueNVS(const char* key) {
    nvs_handle_t my_handle;
    uint8_t value = 0; // Valor por defecto
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        err = nvs_get_u8(my_handle, key, &value);
        nvs_close(my_handle);
        if (err == ESP_OK) {
            return value; // Retornar el valor leído
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            writeSerialComln(String("Clave no encontrada en NVS"));
        } else {
            writeSerialComln(String("Error al leer NVS"));
        }
    } else {
        writeSerialComln(String("Error al abrir NVS"));
    }
    return false; // Retornar false en caso de error
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
    if (menu->id == 7) {
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
        int dutyCycle = data.toInt(); // Convertir el String a entero
        //ToDo//int dc=PWMSetDC(dutyCycle);
        int dc=dutyCycle;
        if (dc>=0 && dc<=100) {
            PWMSetDC(dc); // Cambiar el Duty Cycle
            writeSerialComln(String("Duty Cycle cambiado a: ") + String(dc) + "%");
            
        } else {
            writeSerialComln(String("Valor de Duty Cycle inválido. Debe estar entre 0 y 100."));
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
    if(menu->id ==11){
        int maxDC = data.toInt(); // Convertir el String a entero
        if (PWMSetMaxDC(maxDC)==true) {
            writeSerialComln(String("Valor máximo de Duty Cycle cambiado a: ") + String(maxDC) + "%");
        } else {
            writeSerialComln(String("Valor máximo de Duty Cycle inválido. Debe estar entre 0 y 100."));
        }
    }
    if(menu->id ==17){



    }
    if(menu->id ==19){

        printCargaElectronica();



    }

    if(menu->id ==20){
        int curve, tiempo, value;
        if (parseStringToInts(data.c_str(), &curve, &tiempo, &value)) {
            if (addPointToCurve(curve, tiempo, value) == 0) {
                // OK
            }
        } else {
            Serial.println("❌ Formato inválido. Use [curve,tiempo,value]");
        }
    }
    

    if(menu->id ==15){
        printCargaElectronica();
    }
    if(menu->id ==21){
        if(data.equalsIgnoreCase("y")){
            PWMSetCurveMode(ON_t);
            writeSerialComln(String("Modo curva activado"));
        }else if(data.equalsIgnoreCase("n")){
            PWMSetCurveMode(OFF_t);
            writeSerialComln(String("Modo curva desactivado"));
        }else{
            writeSerialComln(String("Error: Valor invalido"));
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



bool parseStringToInts(const char* str, int* curve, int* tiempo, int* value) {
    if (!str || !curve || !tiempo || !value) return false;

    // Saltar espacios iniciales
    while (isspace((unsigned char)*str)) str++;

    // Debe empezar con '['
    if (*str != '[') return false;
    str++;

    // Leer primer número (curve)
    if (sscanf(str, " %d , %d , %d", curve, tiempo, value) != 3) {
        return false;
    }

    // Verificar que haya ']' al final
    const char* cierre = strrchr(str, ']');
    if (!cierre) return false;

    return true;
}
