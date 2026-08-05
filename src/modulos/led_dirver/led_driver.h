#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#define UPDATE_PERIOD_MS 100

// Estados logicos del sistema, cada uno mapeado a un LED fisico
typedef enum {
    LED_STATE_RUN=0,
    LED_STATE_SENDING_DATA,
    LED_STATE_ERROR,
    LED_STATE_COUNT   // debe quedar siempre al final
} led_state_t;

// Inicializa GPIOs, mutex y apaga todos los LEDs
void led_driver_init(void);

// Marca un estado como activo/inactivo. Si el estado tiene timeout
// configurado, cada llamada con active=true reinicia el contador (heartbeat).
void led_write_state(led_state_t state, bool active);

// Debe llamarse periodicamente (ej. cada 20ms) desde una task propia.
// Actualiza timeouts, blink y hace el shift out al 74HC595 solo si cambio algo.
void led_driver_update(void);
void led_driver_test_blink(uint8_t times, uint16_t on_ms, uint16_t off_ms);
void led_driver_blink_all(void);
#endif // LED_DRIVER_H