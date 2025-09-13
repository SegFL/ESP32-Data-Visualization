#include "time.h"

// Configuración de Wi-Fi
bool saveManualDateTime(int year_, int month_, int day_, int hour_, int minute_, int second_);

static unsigned long millisInit = 0; // milisegundos desde el incio del programa
static unsigned long millisTranscurridos = 0; // milisegundos transcurridos desde el incio del programa
static String timeString = ""; // Variable para almacenar la hora formateada
unsigned long epochTime = 0; // Variable para almacenar el tiempo desde epoch
unsigned long offsetMillis = 0; // Variable para almacenar el tiempo desde epoch
bool WiFiConected=false;
bool isFileCreated=false; 
// Configuración de NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000); // UTC-3 (Argentina)



void enviarComandoCrearArchivo();

void TimeInit() {
    // 1. Intentar conectar WiFi y usar NTP
    if (connectWiFi() == true) {
        timeClient.begin();
        epochTime = timeClient.getEpochTime();
        millisInit = 0; // Guardar el tiempo inicial
        millisTranscurridos = 0; 
        WiFiConected = true;
        enviarComandoCrearArchivo();
        isFileCreated = true;

        // Si hay WiFi/NTP, borrar la fecha manual para que no se use más
        nvs_handle_t handle;
        if (nvs_open("storage", NVS_READWRITE, &handle) == ESP_OK) {
            nvs_erase_key(handle, "manual_epoch");
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
    // 2. Si NO hay WiFi → usar fecha guardada manualmente
    else {
        if (loadManualDateTime()) {
            WiFiConected = false;
            enviarComandoCrearArchivo();
            isFileCreated = true;
        } else {
            // Caso de error: no hay WiFi ni fecha manual
            WiFiConected = false;
            isFileCreated = false;
        }
    }
}

/*
Actualiza el tiempo cada vez que se llama a la función. Usa el tiempo desde epoch
y una diferencia de tiempo para tener los milisegundos transcurridos.

Con epoch time obtengo el tiempoabsoluto y cuandon millisTrancurridos obtengo
el tiempo en milisegundos desde la utlimaacutalizacion de epoch time.
*/


void TimeUpdate() {
    // Si no hay WiFi intentar reconectar
    if (!WiFiConected) {
        if (connectWiFi()) {
            WiFiConected = true;
            if (!isFileCreated) {
                enviarComandoCrearArchivo();
                isFileCreated = true;
            }
        } else {
            return;
        }
    }

    // Actualizar NTP
    if (timeClient.update()) {
        epochTime = timeClient.getEpochTime();
        millisTranscurridos = millis();

        // ⚡️ Avisar a la PC que la hora cambió
        enviarComandoCrearArchivo();
    }

    // Si pasó más de 1 hora desde el último CREATE_FILE
    if (customMillis() > 3600000) {
        enviarComandoCrearArchivo();
    }
}

void enviarComandoCrearArchivo() {
    unsigned long epoch = 0;
    unsigned long millisTranscurridos = 0;
    getTime(epoch, millisTranscurridos);

    // Enviar solo epoch
    writeSerialComln(String("CREATE_FILE") + String(",") + String(epoch));

    offsetMillis = millis(); // Reseteo el contador
}



unsigned long customMillis() {
    return millis() - offsetMillis;
}

void getTime(unsigned long &epoch, unsigned long &millisMedicion) {
    //unsigned long temp=millisMedicion-millisTranscurridos;
    //Calcula la diferencia de tiempo entre la ultima 
    //actualizacion de epoch y el 
    //tiempoen el que se realizo la medicion

    epoch = epochTime;
    millisMedicion = customMillis();//Guarda el resto de los milisegundos
    
}

String getFormattedDateTime() {
    unsigned long epoch, millisTrans;
    getTime(epoch, millisTrans);

    time_t t_epoch = epoch + millisTrans / 1000;  // segundos
    struct tm t;
    localtime_r(&t_epoch, &t);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d %02d:%02d:%02d.%03ld",
             t.tm_year + 1900,
             t.tm_mon + 1,
             t.tm_mday,
             t.tm_hour,
             t.tm_min,
             t.tm_sec,
             millisTrans % 1000);

    return String(buffer);
}



// Guarda fecha manual en NVS
bool saveManualDateTime(int year_, int month_, int day_, int hour_, int minute_, int second_) {
    // Convertir a epoch
    tmElements_t tm;
    tm.Year   = year_ - 1970;  // timeLib cuenta años desde 1970
    tm.Month  = month_;
    tm.Day    = day_;
    tm.Hour   = hour_;
    tm.Minute = minute_;
    tm.Second = second_;
    
    time_t epochManual = makeTime(tm);

    // Guardar en NVS
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_set_u32(handle, "manual_epoch", (uint32_t)epochManual);
    if (err == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);

    // Actualizar variables globales también
    epochTime = epochManual;
    offsetMillis = millis();

    // ⚡️ Avisar a la PC que se actualizó la fecha manualmente
    enviarComandoCrearArchivo();

    return err == ESP_OK;
}

bool loadManualDateTime() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    uint32_t storedEpoch = 0;
    err = nvs_get_u32(handle, "manual_epoch", &storedEpoch);
    nvs_close(handle);

    if (err == ESP_OK) {
        epochTime = storedEpoch;
        offsetMillis = millis();

        // ⚡️Aplicar al reloj del sistema
        struct timeval now = {
            .tv_sec = (time_t)storedEpoch,
            .tv_usec = 0
        };
        settimeofday(&now, NULL);

        return true;
    }
    return false;
}



bool setDateTime(int day, int month, int year, int hour, int minute) {
    struct tm t = {0};

    // struct tm espera: año desde 1900, mes [0-11]
    t.tm_mday = day;
    t.tm_mon  = month - 1;
    t.tm_year = year - 1900;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = 0;

    // Validar fecha
    time_t epoch = mktime(&t);
    if (epoch == -1) {
        return false;  // Fecha/hora inválida
    }

    // Configurar hora del sistema en ESP32
    struct timeval now = {
        .tv_sec = epoch,
        .tv_usec = 0
    };
    if (settimeofday(&now, NULL) != 0) {
        return false;
    }

    // Guardar también en NVS como respaldo
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_set_u32(handle, "manual_epoch", (uint32_t)epoch);
    if (err == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);

    epochTime = epoch;
    offsetMillis = millis();

    // ⚡️ Avisar a la PC que se actualizó la fecha manualmente
    enviarComandoCrearArchivo();

    return err == ESP_OK;
}

