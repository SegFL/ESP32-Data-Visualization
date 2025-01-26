
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
  


typedef enum{
    IDLE,    //estado default
    CONNECTING_WIFI,
    CONNECTING_WIFI_SSID,
    CONNECTING_WIFI_PASSWORD  
} influxdbState_t;

influxdbState_t currentState= IDLE;
//Esta variable sirve para indicar que la funcion encargada de 
//leer datos termino de recivir por lo tanto influxdbUpdate puede pasar
//al siguiente estado correspondiente
//
bool nextState=false;
String buffer="";

void getWiFiCredentials(String &ssid, String &password);
void connectWiFi();
  // Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
  
  // Declare Data point
Point sensor("adc_readings");
  

void influxDBInit(){

    if(serialComChangeState(CONNECTING_WIFI)==true){
        currentState=CONNECTING_WIFI;
    }else{
      writeSerialComln("La intefaz serie esta ocupada, no se pudo connectar a la red WiFi");
      return;
    }

    // Accurate time is necessary for certificate validation and writing in batches
    // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  
  
    // Check server connection
    if (client.validateConnection()) {
        writeSerialCom("Connected to InfluxDB: ");
        writeSerialComln(client.getServerUrl());
    } else {
        writeSerialCom("InfluxDB connection failed: ");
        writeSerialCom(client.getLastErrorMessage());
    }

    sensor.addTag("device", DEVICE);
    // Enable lines batching
    client.setWriteOptions(WriteOptions().batchSize(4));

}


void influxDBUpdate(){

    //Si no se termino de procesar los datos del estado anterior no hago nada
    if( nextState!=true ) {
          switch ( currentState ) {
            case CONNECTING_WIFI:
                connectWiFi();
                 break;
            case CONNECTING_WIFI_SSID:
                influxdbGetChar( receivedChar );
                 break;
            default:
                SerialComState = IDLE;
                break;
        }
    }  

}
void influxDBUpdate1(Buffer* adcBuffer) ;
void influxDBUpdate1(Buffer* adcBuffer) {
  
  ADCData temp;
  while ((adcBuffer->isEmpty()) != true) {
    writeSerialCom("Proceso un Point");
    temp = adcBuffer->popData();
    if(!client.isBufferFull()){
        Point sensor("adc_data");
        sensor.addTag("pin", String(temp.pin));         
        sensor.addField("busVoltage", temp.busVoltage_V);     
        sensor.addField("current", temp.current_mA);     
        sensor.addField("power", temp.power_mW);     
        sensor.addField("shuntVoltage", temp.shuntVoltage_mV);     

        sensor.addField("timestamp", temp.timestamp);   
        client.writePoint(sensor);
    }else{
      if(!client.flushBuffer()){
        writeSerialCom("InfluxDB flush failed: ");
        writeSerialCom(client.getLastErrorMessage()+"\n\r");
        writeSerialCom("Full buffer: ");
        writeSerialCom(client.isBufferFull() ? "Yes" : "No");
        writeSerialCom("\n\r");
      }else{
            writeSerialCom("Dato procesado en influxUpdate():" + String(temp.pin) + "," + String(temp.busVoltage_V) + "\n\r");
      }
    }
    
/*
    if (!client.writePoint(sensor)) {
      writeSerialCom("Error al enviar datos a InfluxDB: ");
      writeSerialCom(client.getLastErrorMessage());
    } else {
      writeSerialCom("Datos enviados a InfluxDB.");
    }
  */ 
    //Borro los valores de los campos del punto
    sensor.clearFields();
  }
}



void connectWiFi(){


     String ssid, password;

    // Obtener las credenciales
    getWiFiCredentials(ssid, password);
}



void getWiFiCredentials(String &ssid, String &password) {
    char receivedChar;
    String inputBuffer = ""; // Almacena los caracteres ingresados temporalmente

    // Solicitar SSID
    writeSerialComln("Ingrese el SSID de la red Wi-Fi y presione Enter:");
    while (true) {
        receivedChar = readSerialChar(); // Leer un carácter desde la terminal
        if (receivedChar == '\n' || receivedChar == '\r') { // Detectar Enter o Retorno de Carro
            if (inputBuffer.length() > 0) {
                ssid = inputBuffer;
                inputBuffer = "";
                break;
            }
        } else {
            inputBuffer += receivedChar; // Agregar el carácter al buffer
        }
    }

    // Solicitar Contraseña
    writeSerialComln("Ingrese la contraseña de la red Wi-Fi y presione Enter:");
    ssid.trim();
    writeSerialComln(ssid);
    while (true) {
        receivedChar = readSerialChar(); // Leer un carácter desde la terminal
        if (receivedChar == '\n' || receivedChar == '\r') { // Detectar Enter o Retorno de Carro
            if (inputBuffer.length() > 0) {
                password = inputBuffer;
                break;
            }
        } else {
            inputBuffer += receivedChar; // Agregar el carácter al buffer
        }
    }

    // Limpieza de caracteres no deseados
    
    password.trim();

    // Confirmar los datos ingresados
    writeSerialComln("Credenciales recibidas:");
    writeSerialCom("SSID: "); writeSerialComln(ssid);
    writeSerialCom("Contraseña: "); writeSerialComln(password);
}


void influxdbGetChar( char receivedChar ){
  if(receivedChar!='\n'){
    buffer+=receivedChar;
  }else{
    nextState=true;
  }
  
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
