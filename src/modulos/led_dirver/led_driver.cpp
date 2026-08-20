#include "led_driver.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <modulos/serialCom/serialCom.h>
#include "config.h"


#define LOAD_LED_PWM_PERIOD_MS 100  // múltiplo de UPDATE_PERIOD_MS, rápido para evitar parpadeo visible


typedef enum {
    LED_MODE_FIXED,
    LED_MODE_BLINK,
    LED_MODE_PWM        
} led_mode_t;

typedef struct {
    uint8_t  bit;
    led_mode_t mode;
    uint16_t blink_period_ms;
    uint16_t timeout_ms;
} led_config_t;

/*
    [LED_STATE_RUN]            = { .bit = 0, .mode = LED_MODE_BLINK, .blink_period_ms = 500,   .timeout_ms = 500    },
    [LED_STATE_SENDING_DATA]   = { .bit = 4, .mode = LED_MODE_BLINK, .blink_period_ms = 200, .timeout_ms = 1000  },
    [LED_STATE_ERROR]          = { .bit = 2, .mode = LED_MODE_BLINK, .blink_period_ms = 500, .timeout_ms = 0    },
*/
static const led_config_t led_config[LED_STATE_COUNT] = {
    [LED_STATE_RUN]            = { .bit = 2, .mode = LED_MODE_BLINK, .blink_period_ms = 500,               .timeout_ms = 500  },
    [LED_STATE_SENDING_DATA]   = { .bit = 4, .mode = LED_MODE_BLINK, .blink_period_ms = 200,               .timeout_ms = 1000 },
    [LED_STATE_LOAD_0]         = { .bit = 0, .mode = LED_MODE_PWM,   .blink_period_ms = LOAD_LED_PWM_PERIOD_MS, .timeout_ms = 0 },
    [LED_STATE_LOAD_1]         = { .bit = 1, .mode = LED_MODE_PWM,   .blink_period_ms = LOAD_LED_PWM_PERIOD_MS, .timeout_ms = 0 },
    [LED_STATE_LOAD_2]         = { .bit = 3, .mode = LED_MODE_PWM,   .blink_period_ms = LOAD_LED_PWM_PERIOD_MS, .timeout_ms = 0 },
    [LED_STATE_LOAD_3]         = { .bit = 5, .mode = LED_MODE_PWM,   .blink_period_ms = LOAD_LED_PWM_PERIOD_MS, .timeout_ms = 0 },
    [LED_STATE_LOAD_4]         = { .bit = 6, .mode = LED_MODE_PWM,   .blink_period_ms = LOAD_LED_PWM_PERIOD_MS, .timeout_ms = 0 },
    // bit 7 queda libre
};

static uint8_t  led_active_flags = 0;
static uint16_t timeout_remaining[LED_STATE_COUNT] = {0};
static uint16_t blink_counters[LED_STATE_COUNT] = {0};
static uint8_t  led_shadow_register = 0;
static uint8_t  led_last_sent = 0xFF;
static SemaphoreHandle_t led_mutex;
static uint8_t led_duty[LED_STATE_COUNT] = {0}; // duty runtime, solo usado en modo PWM


static void led_hc595_shift_out(uint8_t data);
void led_set_duty(led_state_t state, uint8_t duty_percent);

void led_driver_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_SER) | (1ULL << PIN_SCLK) | (1ULL << PIN_RCLK),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    gpio_set_level(PIN_SER, 0);
    gpio_set_level(PIN_SCLK, 0);
    gpio_set_level(PIN_RCLK, 0);

    led_mutex = xSemaphoreCreateMutex();

    led_hc595_shift_out(0);
}

// ---- Rutina de test de hardware: titila todos los LEDs N veces ----
// Bypassea la maquina de estados (led_active_flags) y escribe directo al 595.
// Util para validar cableado/soldadura antes de correr la logica normal.
void led_driver_test_blink(uint8_t times, uint16_t on_ms, uint16_t off_ms)
{
    for (uint8_t i = 0; i < times; i++) {
        led_hc595_shift_out(0xFF); // todos encendidos
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        led_hc595_shift_out(0x00); // todos apagados
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
    led_last_sent = 0xFF; // fuerza reenvio en el primer led_driver_update() posterior
}

void led_write_state(led_state_t state, bool active)
{
    if (state >= LED_STATE_COUNT) return;
    uint8_t bit = led_config[state].bit;

    xSemaphoreTake(led_mutex, portMAX_DELAY);
    if (active) {
        led_active_flags |= (1 << bit);
        timeout_remaining[state] = led_config[state].timeout_ms;
    } else {
        led_active_flags &= ~(1 << bit);
        timeout_remaining[state] = 0;
    }
    xSemaphoreGive(led_mutex);
}
void led_driver_update(void)
{
    uint8_t active_snapshot;

    xSemaphoreTake(led_mutex, portMAX_DELAY);

    for (int s = 0; s < LED_STATE_COUNT; s++) {
        if (led_config[s].timeout_ms == 0) continue;
        if (timeout_remaining[s] == 0) continue;

        if (timeout_remaining[s] > UPDATE_PERIOD_MS) {
            timeout_remaining[s] -= UPDATE_PERIOD_MS;
        } else {
            timeout_remaining[s] = 0;
            led_active_flags &= ~(1 << led_config[s].bit);
        }
    }

    active_snapshot = led_active_flags;
    xSemaphoreGive(led_mutex);

    uint8_t output = 0;

    for (int s = 0; s < LED_STATE_COUNT; s++) {
        uint8_t bit = led_config[s].bit;
        bool is_active = (active_snapshot >> bit) & 0x01;
        if (!is_active) continue;

        switch (led_config[s].mode) {

            case LED_MODE_FIXED:
                output |= (1 << bit);
                break;

            case LED_MODE_BLINK:
                blink_counters[s] += UPDATE_PERIOD_MS;
                if (blink_counters[s] >= led_config[s].blink_period_ms) {
                    blink_counters[s] = 0;
                }
                if (blink_counters[s] < (led_config[s].blink_period_ms / 2)) {
                    output |= (1 << bit);
                }
                break;

            case LED_MODE_PWM: {
                blink_counters[s] += UPDATE_PERIOD_MS;
                if (blink_counters[s] >= led_config[s].blink_period_ms) {
                    blink_counters[s] = 0;
                }
                uint16_t on_time = (uint16_t)((uint32_t)led_config[s].blink_period_ms * led_duty[s] / 100);
                if (blink_counters[s] < on_time) {
                    output |= (1 << bit);
                }
                break;
            }
        }
    }

    led_shadow_register = output;

    if (led_shadow_register != led_last_sent) {
        led_hc595_shift_out(led_shadow_register);
        led_last_sent = led_shadow_register;
    }
}


static void led_hc595_shift_out(uint8_t data)
{
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(PIN_SER, (data >> i) & 0x01);
        gpio_set_level(PIN_SCLK, 1);
        gpio_set_level(PIN_SCLK, 0);
    }
    gpio_set_level(PIN_RCLK, 1);
    gpio_set_level(PIN_RCLK, 0);
}

// Titila todas las salidas del 595 a 5Hz (100ms on / 100ms off) en forma continua.
// Llamar una sola vez desde una task; no retorna.
void led_driver_blink_all(void)
{
    const TickType_t half_period = pdMS_TO_TICKS(100); // 100ms -> 5Hz de ciclo completo
    bool state = false;

    for (;;) {
        writeSerialComln("Blinking all LEDs at 5Hz. Press reset to exit.");
        state = !state;
        led_hc595_shift_out(state ? 0xFF : 0x00);
        vTaskDelay(half_period);
    }
}

void led_set_duty(led_state_t state, uint8_t duty_percent) {
    if (state >= LED_STATE_COUNT) return;
    if (duty_percent > 100) duty_percent = 100;

    xSemaphoreTake(led_mutex, portMAX_DELAY);
    led_duty[state] = duty_percent;
    if (duty_percent > 0) {
        led_active_flags |= (1 << led_config[state].bit); // activa el bit para que el loop lo procese
    } else {
        led_active_flags &= ~(1 << led_config[state].bit);
    }
    xSemaphoreGive(led_mutex);
}