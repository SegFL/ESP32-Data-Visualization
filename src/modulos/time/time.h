#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <modulos/serialCom/serialCom.h>
#include <TimeLib.h>  // Librería para manejar fecha y hora
void TimeInit();
void TimeUpdate() ;
String getFormattedDateTime();