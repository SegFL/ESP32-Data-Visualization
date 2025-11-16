#include "WiFi.h"
#include <modulos/serialCom/serialCom.h>

String ssid = "Claro";
String password = "17727630";





bool connectWiFi() {

    //Si tengo valores ssid-password guardados en NVS los uso
    char ssidBuf[64];//tienen que ser de 64 pq esta hardcoedeado asi en nsv.cpp
    char passBuf[64];

    esp_err_t err_ssid = readStringNVS("WIFI_SSID", ssidBuf);
    esp_err_t err_pass = readStringNVS("WIFI_PASS", passBuf);

    if (err_ssid == ESP_OK && err_pass == ESP_OK) {
        ssid     = String(ssidBuf);
        password = String(passBuf);
    }




    WiFi.begin(ssid.c_str(), password.c_str());
    writeSerialComln(String("Conectando a Wi-Fi..."));

    // Esperar hasta 5 segundos para conectar
    int maxRetries = 20;
    while (WiFi.status() != WL_CONNECTED && maxRetries-- > 0) {
        delay(500);
        writeSerialCom(String("."));
    }

    if (WiFi.status() == WL_CONNECTED) {
        writeSerialComln(String("\nConectado a Wi-Fi"));
        return true;
    } else {
        writeSerialComln(String("\nError al conectar a Wi-Fi"));
        return false;
    }
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