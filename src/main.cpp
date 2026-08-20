#include <Arduino.h>
#include "modulos/adc/adc.h"
#include "modulos/serialCom/serialCom.h"
#include "modulos/carga_electronica/carga_electronica.h"
#include "modulos/userInterface/userInterface.h"
#include "modulos/queueCom/queueCom.h"
#include "modulos/ina219/ina219.h"
#include "modulos/time/time.h"
#include "modulos/dac/dac.h"
#include "modulos/led_dirver/led_driver.h"
#include "esp_timer.h"
#include "config.h"


#define QUEUE_LENGTH 100
#define ITEM_SIZE sizeof(char)



void run_led_init(uint64_t period_us);
static void run_led_toggle_cb(void* arg);
void run_led_set_period(uint64_t period_us);
static esp_timer_handle_t run_led_timer;
#define BLINK_FAST_US 50000   // 100ms → 5 Hz mientras inicializa
#define BLINK_SLOW_US 250000   // 250ms → 2 Hz en operación normal

static bool run_led_state = false;


QueueHandle_t xQueueComSerial;
QueueHandle_t timeRequestQueue;

TaskHandle_t Task1Handle = NULL;
TaskHandle_t Task2Handle = NULL;

void Task1(void *pvParameters) {
  int received;
  for (;;) {
    if (xQueueReceive(timeRequestQueue, &received, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (received == 1) {
        // Evitamos llamadas pesadas aquí
      }
    }

    while (Serial.available() > 0) {
        userInterfaceUpdate();
    }
    TimeUpdate();
    vTaskDelay(pdMS_TO_TICKS(200)); // Cede CPU al resto de tareas
  }
}

/*
Task2 (Core 1)
├── leerADC()
│   ├── getData(sensor 0)  ← I2C ~1ms
│   │   └── writeSerialComlnDATA()  ← String + malloc x6
│   ├── getData(sensor 1)  ← I2C ~1ms
│   │   └── writeSerialComlnDATA()
│   └── ... x5 sensores
│   └── lastCurrent_mA[i] = temp.current_mA  ← variable global sin protección
│
└── CargaElectronicaUpdate()
    └── getLastCurrentData(i)  ← lee lastCurrent_mA[] sin protección



*/






void TaskLed(void *pvParameters)
{



    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(UPDATE_PERIOD_MS); // debe matchear el define del driver (100ms)
    led_write_state(LED_STATE_RUN,true);
    for (;;) {
        led_write_state(LED_STATE_RUN, true); // kick constante

        led_driver_update();
        vTaskDelayUntil(&lastWake, period);
    }
}

void TaskSensors(void *pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(100);

    for (;;) {
        leerADC();
        vTaskDelayUntil(&lastWake, period);
    }
}
//Tarea que solo controla el PID
void TaskControl(void *pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(100); // PID puede correr más seguido

    for (;;) {
        CargaElectronicaUpdate();
        vTaskDelayUntil(&lastWake, period);
    }
}



void setup() {

  //Pongo el led de RUN a parpadear rápido mientras inicializa
  run_led_init(BLINK_FAST_US);   


  userInterfaceInit();
  writeSerialComln("=== INICIO DEL SISTEMA ===");
  writeSerialComln(String("Memoria inicial: ") + ESP.getFreeHeap());
  nvsInit();
  vTaskDelay(pdMS_TO_TICKS(100));

  queueInit();
  ina219Init();
  adcInit();
  TimeInit();
  CargaElectronicaInit();
  dacInit();

  led_driver_init();

  //Pongo el led de RUN a parpadear lento
  run_led_set_period(BLINK_SLOW_US); 


  xQueueComSerial = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);
  timeRequestQueue = xQueueCreate(10, sizeof(int));

  xTaskCreatePinnedToCore(Task1, "Task1", 2*4096, NULL, 1, &Task1Handle, 0);
  //Task2 tendra la priordad maxima ya que se encarga se leer y escribir entradas/salidas

  xTaskCreatePinnedToCore(TaskSensors, "Sensors", 2*4096, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(TaskControl, "Control", 2*4096, NULL, 5, NULL, 1); 
  xTaskCreatePinnedToCore(TaskLed, "Led", 2*4096, NULL, 3, NULL, 1);

  writeSerialComln("Tareas inicializadas");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}


static void run_led_toggle_cb(void* arg) {
    run_led_state = !run_led_state;
    digitalWrite(LED_RUN_PIN, run_led_state);
}

void run_led_init(uint64_t period_us) {
    pinMode(LED_RUN_PIN, OUTPUT);
    const esp_timer_create_args_t args = {
        .callback = &run_led_toggle_cb,
        .name = "run_led"
    };
    esp_timer_create(&args, &run_led_timer);
    esp_timer_start_periodic(run_led_timer, period_us);
}

void run_led_set_period(uint64_t period_us) {
    esp_timer_stop(run_led_timer);
    esp_timer_start_periodic(run_led_timer, period_us);
}