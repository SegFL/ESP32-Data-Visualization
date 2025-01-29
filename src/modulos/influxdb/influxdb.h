

typedef enum{
    IDLE,    //estado default
    CONNECTING_WIFI,
    CONNECTING_WIFI_SSID,
    CONNECTING_WIFI_PASSWORD  
} influxdbState_t;
void influxDBInit();
void writeData(uint16_t);
void influxDBUpdate(Buffer*);
