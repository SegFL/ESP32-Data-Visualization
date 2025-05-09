#include "time.h"

// Configuración de Wi-Fi
const char *ssid = "Claro";
const char *password = "17727630";
static String timeString = ""; // Variable para almacenar la hora formateada
// Configuración de NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000); // UTC-3 (Argentina)

// Función para inicializar la conexión Wi-Fi y NTP
void TimeInit() {

    WiFi.begin(ssid, password);
    writeSerialComln("Conectando a Wi-Fi...");
    if (WiFi.status() != WL_CONNECTED) {
        writeSerialCom("Error al conectar a Wi-Fi: ");
    }
    writeSerialComln("\nConectado a Wi-Fi");
    timeClient.begin();
}

// Función para actualizar y obtener la hora
void TimeUpdate() {
    timeClient.update();
    timeString = timeClient.getFormattedTime(); // Obtener la hora formateada
    return ;
}

String getFormattedDateTime() {
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();

    // Establecemos el tiempo en la librería TimeLib
    setTime(epochTime);

    // Construimos el formato: DD/MM/YYYY HH:MM:SS
    String formattedDateTime = String(day()) + "/" + 
                               String(month()) + "/" + 
                               String(year()) + " " + 
                               String(hour()) + ":" + 
                               String(minute()) + ":" + 
                               String(second());

    return formattedDateTime;
}

