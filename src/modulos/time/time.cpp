#include "time.h"

// Configuración de Wi-Fi

static unsigned long millisInit = 0; // milisegundos desde el incio del programa
static unsigned long millisTranscurridos = 0; // milisegundos transcurridos desde el incio del programa
static String timeString = ""; // Variable para almacenar la hora formateada
unsigned long epochTime = 0; // Variable para almacenar el tiempo desde epoch
bool WiFiConected=false;
// Configuración de NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000); // UTC-3 (Argentina)

// Función para inicializar la conexión Wi-Fi y NTP
void TimeInit() {

    if(connectWiFi()==true){
        writeSerialComln("Conectado a Wi-Fi");
        timeClient.begin();
        epochTime = timeClient.getEpochTime();
        millisInit = 0; // Guardar el tiempo inicial
        millisTranscurridos = 0; 
        WiFiConected=true;
    }
}
/*
Actualiza el tiempo cada vez que se llama a la función. Usa el tiempo desde epoch
y una diferencia de tiempo para tener los milisegundos transcurridos.

Con epoch time obtengo el tiempoabsoluto y cuandon millisTrancurridos obtengo
el tiempo en milisegundos desde la utlimaacutalizacion de epoch time.
*/


void TimeUpdate() {
    if(WiFiConected==false){
        if(connectWiFi()==true){
            WiFiConected=true;
        }else{
            
            return;
        }
        
    }
    timeClient.update();
    epochTime = timeClient.getEpochTime();
    millisTranscurridos = millis() - millisInit; // Calcular el tiempo transcurrido desde el inicio
    return ;
}

void getTime(unsigned long &epoch, unsigned long &millisTrans) {
    epoch = epochTime+millisTranscurridos/1000;
    millisTrans = millisTranscurridos%1000;
    
}


// Formatear la fecha y hora como "YYYY/MM/DD HH:MM:SS.MMMM"
String getFormattedDateTime() {
    unsigned long epoch, millisTrans;
    getTime(epoch, millisTrans);

    String formattedTime = String(year(epoch)) + "/" +
                             String(month(epoch)) + "/" +
                             String(day(epoch)) + " " +
                             String(hour(epoch)) + ":" +
                             String(minute(epoch)) + ":" +
                             String(second(epoch))+ "."+
                             String(millisTrans);
    // Formatear la fecha y hora como "YYYY/MM/DD HH:MM:SS"
    



    return formattedTime;
}


