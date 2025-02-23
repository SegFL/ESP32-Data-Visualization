

#include "adc.h"


//Este flag se usa para saber si es necesario pasar los datos a la interfaz de usuario
bool userInterfaceAvailable=false;

void adcInit() {
    // Inicializar el buffer

    // Configuración de pines
    pinMode(36, INPUT);
}




void  leerADC(){


  ADCData temp;
  int i =0;
  while(i<1){
    
    if(getData(temp,i)==false){//Leo cada uno de los sensores(3)
        temp.pin=i; 
        temp.timestamp=micros();
        //adcBuffer->addData(temp);
        //writeSerialComln(String(temp.busVoltage_V));
        //temp.busVoltage_V=analogRead(36);
        //if(sendSensorDataToUserInterface(temp)==true){//Envia datos a la interfaz de usuario
          //writeSerialCom("Datos enviados a la interfaz de usuario");
        //}

    i++;
    }

  
  }
}






