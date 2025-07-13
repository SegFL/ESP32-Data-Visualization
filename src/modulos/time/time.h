#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <modulos/serialCom/serialCom.h>
#include <TimeLib.h>  // Librería para manejar fecha y hora
#include <modulos/WiFi/WiFi.h>
void TimeInit();
void TimeUpdate() ;
void getTime(unsigned long &epoch, unsigned long &millisTrans);
String getFormattedDateTime();