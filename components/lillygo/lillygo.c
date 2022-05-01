#include "lillygo.h"
#include <stdint.h>
#include <stdio.h>

#include "esp_bit_defs.h"
#include "driver/gpio.h"

void
lillygo__led_switch_pwr(bool turnON)
{
    if (turnON == true)
        gpio_set_level(LILLYGO_LED_GPIO_NUM, 1);
    else
        gpio_set_level(LILLYGO_LED_GPIO_NUM, 0);
}

void
lillygo__sim800_switch_pwr(bool turnON)
{
    if (turnON == SIM800_PWR_ON)
        gpio_set_level(LILLYGO_SIM800_PWR_GPIO_NUM, 1);
    else
        gpio_set_level(LILLYGO_SIM800_PWR_GPIO_NUM, 0);
}

void
lillygo__config_gpio(void)
{
    gpio_config_t conf;

    conf.pin_bit_mask = (BIT(LILLYGO_SIM800_PWR_GPIO_NUM) | BIT(LILLYGO_LED_GPIO_NUM));
    conf.mode         = GPIO_MODE_OUTPUT;
    conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    conf.intr_type    = GPIO_INTR_DISABLE;

    gpio_reset_pin(BIT(LILLYGO_SIM800_PWR_GPIO_NUM));
    gpio_reset_pin(BIT(LILLYGO_LED_GPIO_NUM));
    gpio_config(&conf);

    lillygo__sim800_switch_pwr(SIM800_PWR_OFF);
    lillygo__led_switch_pwr(false);
}
