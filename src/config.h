


#define NUMBER_OF_SENSORS 5// Número de sensores INA219: Si se cambia tambien se deberia cambiar el valor en ins219.cpp
#define NUMBER_OF_ELECTRONIC_LOADS 5 // Número de cargas electronicas (PWM) que se controlan. (4 + 1 para pruebas auxiliares)


#define ESP32_WROOM
////////////////////////////////////PINES
#ifdef ESP32_WROOM      //Conversor usb 38 PINES
    #define PWM_0_PIN 17
    #define PWM_1_PIN 16
    #define PWM_2_PIN 4
    #define PWM_3_PIN 18
    #define PWM_4_PIN 19 //En realidad no se usa ni esta routeado
    #define I2C_SDA_PIN 13
    #define I2C_SCL_PIN  27
    // ---- Pines del 74HC595 ----
    #define PIN_RCLK   GPIO_NUM_22
    #define PIN_SCLK  GPIO_NUM_21
    #define PIN_SER  GPIO_NUM_23

    #define LED_RUN_PIN 2   // Led de RUN de la DevBoard



#else //ESP32  DEVKIT1 30PINES  
    #define PWM_0_PIN 16
    #define PWM_1_PIN 19
    #define PWM_2_PIN 21
    #define PWM_3_PIN 22
    #define PWM_4_PIN 32
    #define I2C_SDA_PIN 13
    #define I2C_SCL_PIN 14
    // ---- Pines del 74HC595 ----
    #define PIN_RCLK   GPIO_NUM_17
    #define PIN_SCLK  GPIO_NUM_4
    #define PIN_SER  GPIO_NUM_18


#endif
/*
version vieja 



#define NUMBER_OF_SENSORS 5// Número de sensores INA219: Si se cambia tambien se deberia cambiar el valor en ins219.cpp
#define NUMBER_OF_ELECTRONIC_LOADS 5 // Número de cargas electronicas (PWM) que se controlan. (4 + 1 para pruebas auxiliares)


#define ESP32_WROOM
////////////////////////////////////PINES
#ifdef ESP32_WROOM      //Conversor usb 38 PINES
    #define PWM_0_PIN 5
    #define PWM_1_PIN 21
    #define PWM_2_PIN 3
    #define PWM_3_PIN 23
    #define PWM_4_PIN 35
    #define I2C_SDA_PIN 12
    #define I2C_SCL_PIN  27


#else //ESP32  DEVKIT1 30PINES  
    #define PWM_0_PIN 16
    #define PWM_1_PIN 19
    #define PWM_2_PIN 21
    #define PWM_3_PIN 22
    #define PWM_4_PIN 32
    #define I2C_SDA_PIN 13
    #define I2C_SCL_PIN 14

#endif
/*



*/

