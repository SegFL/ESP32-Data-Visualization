#include "WiFi.h"
#include <modulos/serialCom/serialCom.h>

String ssid = "Claro";
String password = "17727630";




void wifi_timer_callback(TimerHandle_t xTimer);
TimerHandle_t wifi_timer;

int wifiRetryCount = 0;
int wifiMaxRetries = 20;
bool wifiRetrying = false; // Variable para saber si estoy reintentando conectar o no
bool wifiConnected = false;
void connectWiFi() {

    if(wifiRetrying==true){
        return; // Si ya estoy reintentando conectar, no hago nada. El timer/hanlder ya esta activo
    }
    wifiRetryCount = 0;

    wifi_timer = xTimerCreate(
        "wifi_retry",
        pdMS_TO_TICKS(5000),
        pdTRUE,      // repetitivo
        NULL,
        wifi_timer_callback
    );

    char ssidBuf[64];
    char passBuf[64];

    esp_err_t err_ssid = readStringNVS("WIFI_SSID", ssidBuf);
    esp_err_t err_pass = readStringNVS("WIFI_PASS", passBuf);

    if (err_ssid == ESP_OK && err_pass == ESP_OK) {
        ssid     = String(ssidBuf);
        password = String(passBuf);
    }

    writeSerialComln("Intentando conectar WiFi");

    WiFi.begin(ssid.c_str(), password.c_str());

    xTimerStart(wifi_timer, 0);

  
}


void setSSID(String newSSID) {
    ssid = newSSID;  // guardar en RAM

    // guardar en NVS
    saveStringNVS("WIFI_SSID", ssid.c_str());


}

void setPassWord(String newPassword) {
    password = newPassword;  // guardar en RAM

    // guardar en NVS
    esp_err_t err = saveStringNVS("WIFI_PASS", password.c_str());


}
//Devuelve los valores de SSID y PASSWORD guardados en RAM
void getWiFiCredentials(String &outSSID, String &outPassword) {
    // Asignar los valores guardados a los parámetros de referencia
    outSSID = ssid;
    outPassword = password;
}

void wifi_timer_callback(TimerHandle_t xTimer){

    if (WiFi.status() == WL_CONNECTED) {
        xTimerStop(wifi_timer,0);
        wifiRetrying=false;
        wifiConnected=true;
        return;
    }

    wifiRetryCount++;
    WiFi.reconnect();
}

bool checkWiFi(){

    if(wifiConnected){
        return true;
    }
    if (WiFi.status() == WL_CONNECTED) {

        xTimerStop(wifi_timer,0);
        wifiConnected = true;
        wifiRetrying = false;
        return true;
    }

    return false;

}

