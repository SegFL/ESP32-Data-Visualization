
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"

  
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <modulos/serialCom/serialCom.h>
#include <modulos/buffer/buffer.h>
#include "ADCData.h"
#include "influxdb.h"
#include <modulos/adc/adc.h>
  // WiFi AP SSID
#define WIFI_SSID "Claro"
  // WiFi password
#define WIFI_PASSWORD "17727630"
  
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "1Hp2xWGjEg3MgmKIErnqLVZYtc4JhKcfrN8SQSI6ALxZhgqgOjtaHajA2wKWjSqxTfFM_f-bKgnmkHbfd2BiGw=="
#define INFLUXDB_ORG "0091727f0b3afd7a"
#define INFLUXDB_BUCKET "adcData0"




  
  // Time zone info
#define TZ_INFO "UTC-3"
  
  // Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
  
  // Declare Data point
Point sensor("adc_readings");
  


void influxDBInit(){


    
    // Setup wifi
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
    writeSerialCom("Connecting to wifi");
    while (wifiMulti.run() != WL_CONNECTED) {
      writeSerialCom(".");
      delay(100);
    }
    Serial.println();
  
    // Accurate time is necessary for certificate validation and writing in batches
    // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  
  
    // Check server connection
    if (client.validateConnection()) {
        writeSerialCom("Connected to InfluxDB: ");
        writeSerialCom(client.getServerUrl()+"\n\r");
    } else {
        writeSerialCom("InfluxDB connection failed: ");
        writeSerialCom(client.getLastErrorMessage());
    }

    sensor.addTag("device", DEVICE);

}

void writeData(uint16_t data){
       // Clear fields for reusing the point. Tags will remain the same as set above.
    sensor.clearFields();
  
    // Store measured value into point
    // Report RSSI of currently connected network


    // Print what are we exactly writing
    if (!client.writePoint(sensor)) {
        writeSerialCom("InfluxDB write failed: ");
        writeSerialCom(client.getLastErrorMessage());
    } else {
        writeSerialCom("Datos enviados a InfluxDB:");
        writeSerialCom(sensor.toLineProtocol()); // Imprimir lo enviado para depuración
    }
    // Check WiFi connection and reconnect if needed
    if (wifiMulti.run() != WL_CONNECTED) {
      writeSerialCom("Wifi connection lost");
    }
  

  


}


void influxDBUpdate(Buffer* adcBuffer) {
  
  ADCData temp;
  while ((adcBuffer->isEmpty()) != true) {
    sensor.clearFields();

    temp = adcBuffer->popData();
    writeSerialCom("Dato procesado en influxUpdate():" + String(temp.pin) + "," + String(temp.value) + "\n\r");
    
    // Crear un punto para enviar a InfluxDB
    Point sensor("adc_data");
    
    // Convertir pin a cadena antes de agregarlo como etiqueta
    sensor.addTag("pin", String(temp.pin));          // Agregar pin como etiqueta
    
    // Asegurarse de que 'value' sea tratado como un número (int o float según corresponda)
    sensor.addField("value", (float)temp.value);      // Agregar el valor del ADC como un campo numérico (float)
    sensor.addField("timestamp", temp.timestamp);     // Agregar la marca de tiempo
    
    if (!client.writePoint(sensor)) {
      writeSerialCom("Error al enviar datos a InfluxDB: ");
      writeSerialCom(client.getLastErrorMessage());
    } else {
      writeSerialCom("Datos enviados a InfluxDB.");
    }
  }
}

