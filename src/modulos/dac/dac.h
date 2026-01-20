
#include "driver/dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"



#define DAC_CH   DAC_CHANNEL_1   // GPIO25
#define DAC_MAX  255

void dacUpdate();
void dacInit();
 void setDacValue(uint8_t value);
 uint8_t getDACValue(void);