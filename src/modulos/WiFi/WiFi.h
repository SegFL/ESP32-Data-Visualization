
#ifndef WIFI_H
#define WIFI_H
#include <WiFi.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/nvs/nvs.h>
bool connectWiFi();
void getWiFiCredentials(String &ssid, String &password);
void setPassWord(String newPassword);
void setSSID(String newSSID);

#endif // WIFI_H


