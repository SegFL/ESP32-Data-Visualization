
#ifndef WIFI_H
#define WIFI_H
#include <WiFi.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/nvs/nvs.h>
void connectWiFi();
bool checkWiFi();
void getWiFiCredentials(String &ssid, String &password);
void setPassWord(String newPassword);
void setSSID(String newSSID);

#endif // WIFI_H


