
#ifndef WIFI_H
#define WIFI_H

#include <modulos/serialCom/serialCom.h>

bool connectWiFi();
void getWiFiCredentials(String &ssid, String &password);
void setPassWord(String newPassword);
void setSSID(String newSSID);

#endif // WIFI_H


