#include "../dac/dac.h"

 
#define DAC_MAX 255

static uint8_t dacValue = 0;
static bool dacDirty = false;   // indica que hay un nuevo valor





void dacInit() {
    dac_output_enable(DAC_CH);
    dac_output_voltage(DAC_CH, 0);
}

void dacUpdate(void)
{
    if (!dacDirty)
        return;

    dac_output_voltage(DAC_CH, dacValue);
    dacDirty = false;
}

void setDacValue(uint8_t value)
{
    if (value > DAC_MAX)
        value = DAC_MAX;

    dacValue = value;
    dacDirty = true;   // marca que hay que actualizar el DAC
}
uint8_t getDACValue(void)
{
    return dacValue;
}