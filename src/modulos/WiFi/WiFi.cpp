#include <WiFi.h>
#include <modulos/serialCom/serialCom.h>

String ssid = "Claro";
String password = "17727630";





bool connectWiFi() {
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
    ssid = newSSID;
}

void setPassWord(String newPassword) {
    password = newPassword;
}

void getWiFiCredentials(String &outSSID, String &outPassword) {
    // Asignar los valores guardados a los parámetros de referencia
    outSSID = ssid;
    outPassword = password;
}