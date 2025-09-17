#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <modulos/serialCom/serialCom.h>
#include <TimeLib.h>  // Librería para manejar fecha y hora
#include <modulos/WiFi/WiFi.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <TimeLib.h>  // Para convertir fecha a epoch

void TimeInit();
void TimeUpdate() ;
void getTime(unsigned long &epoch, unsigned long &millisTrans);
String getFormattedDateTime();
bool loadManualDateTime();
bool setDateTime(int day, int month, int year, int hour, int minute);

//Envia el tiempo en milisegundos desde el la ultima vez que se envio elcomando de crear archivo
unsigned long customMillis();
unsigned long getCurrentEpoch();

