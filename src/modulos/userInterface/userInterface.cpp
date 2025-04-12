#include <nvs.h>

#include "userInterface.h"
#include <ADCData.h>
#include "modulos/queueCom/queueCom.h"
#include <modulos/PWM/PWM.h>


#define MAX_DATA_BUFFER 30
#define ESC 27
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

void userInterfaceInit(){
    serialComInit();
    clearScreen();//Borra mensajes del ESP32 al iniciar el programa
    menu=menuInit();
    if(menu){
        writeSerialComln("Menu inicializado");
    }else{
        writeSerialComln("Menu no inicializado");
    }
    if(loadConfiguration()){
        writeSerialComln("Configuracion cargada correctamente");
    } else{
        writeSerialComln("Error al cargar la configuracion");
    }

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

    if(charReceived=='\n'){//Si se presiona enter se cambia el estado de recvir datos
        if(aceptandoDatos==true){
            aceptandoDatos=false;
            writeSerialComln("Datos recibidos");
            writeSerialComln(String(data_buffer));
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
        writeSerialComln("Modo SEND DATA activado");
        return true;
    } else {
        changeMode(NOT_SEND_DATA); // Cambiar el modo a RECEIVE_DATA
        writeSerialComln("Modo SEND DATA desactivado");
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
        writeSerialComln("Error al abrir NVS");
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
            writeSerialComln("Clave no encontrada en NVS");
        } else {
            writeSerialComln("Error al leer NVS");
        }
    } else {
        writeSerialComln("Error al abrir NVS");
    }
    return false; // Retornar false en caso de error
}



void procesarDatos(String data) {
    if (data.isEmpty() || menu == nullptr) { // Mejor forma de validar String vacío
        return;
    }
    if (menu->id == 3) {
        writeSerialComln("Llame a la funcion cargar contraseña");
    }
    if (menu->id == 6) {
        if (data.equalsIgnoreCase("y")) { // Comparación más eficiente
            changeMode(SEND_DATA); // Cambiar el modo a SEND_DATA
            writeSerialComln("Modo SEND DATA activado");
            saveValueNVS("mode", SEND_DATA); // Guardar el modo en NVS
        }
        if (data.equalsIgnoreCase("n")) { 
            changeMode(NOT_SEND_DATA); 
            writeSerialComln("Modo SEND DATA desactivado");
            saveValueNVS("mode", NOT_SEND_DATA); 
        }
    }

    if(menu->id==8){
        int dutyCycle = data.toInt(); // Convertir el String a entero
        if (PWMSetDC(dutyCycle)==true) {
            writeSerialComln("Duty Cycle cambiado a: " + String(dutyCycle) + "%");
        } else {
            writeSerialComln("Valor de Duty Cycle inválido. Debe estar entre 0 y 100.");
        }
    }
    if(menu->id ==9){
        int frequency = data.toInt(); // Convertir el String a entero
        if (PWMSetFrequency(frequency)==true) {
            writeSerialComln("Frecuencia cambiada a: " + String(frequency) + " Hz");
        } else {
            writeSerialComln("Valor de frecuencia inválido. Debe ser mayor que 0.");
        }
    }

    
}




void printSensorData() {


    const int cant=4;
    ADCData data[cant]={};

    if (receiveSensorDataToUserInterface(data)) {
        for (int i = 0; i < cant; i++){
            printSensor(data[i]);
            writeSerialComln("\n");
        }
        
    } 
  


}

void printSensor(ADCData data){
    writeSerialComln(String("Pin: ") + String(data.pin));
    writeSerialComln(String("\tBus Voltage: ") + String(data.busVoltage_V) + String(" V"));
    writeSerialComln(String("\tShunt Voltage: ") + String(data.shuntVoltage_mV) + String(" mV"));
    writeSerialComln(String("\tCurrent: ") + String(data.current_mA) + String(" mA"));
    writeSerialComln(String("\tPower: ") + String(data.power_mW) + String(" mW"));
    writeSerialComln(String("\tTimestamp: ") + String(data.timestamp));//Provoca problemas con punteros nulos o de memoria

}