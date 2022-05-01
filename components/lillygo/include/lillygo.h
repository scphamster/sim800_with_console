#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_ERR_BUFLEN 30

#define LILLYGO_LED_GPIO_NUM 13

#define LILLYGO_SIM800_PWR_GPIO_NUM 23
#define SIM800_PWR_ON     true
#define SIM800_PWR_OFF    false

/**
 * @brief switch power of blue onboard led
 * 
 * @param turnON true = ON, false = OFF
 */
void lillygo__led_switch_pwr(bool turnON);
/**
 * @brief Switch power of sim800L module on board
 * 
 * Controls the dcdc converter on board (SY8089). 
 * GPIO num LILLYGO_SIM800_PWR_GPIO_NUM is connected to SY8089 EN(1) pin.
 * Output of converter is 4.4V supplied to pins BAT1(1) and BAT2(42)
 * 
 * @param turnON true = ON, false = OFF
 */
void lillygo__sim800_switch_pwr(bool turnON);
/**
 * @brief set up gpio needed to control SIM800 power and onboard LED
 * 
 */
void lillygo__config_gpio(void);

#ifdef __cplusplus
}
#endif