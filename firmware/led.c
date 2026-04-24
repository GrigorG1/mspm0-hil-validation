/*
 * ======== led.c ========
 * LED driver: green, yellow and red on the pins below.
 * All three are active-high (GPIO high = LED on).
 * Port A and Port B power must already be enabled (HIL_Hardware_Init does this).
 *
 * Note: PA26 was tried but it did not work easily and for the sake of time
 * switched to PA8.
 */

#include <ti/driverlib/dl_gpio.h>
#include "led.h"

// Green LED: PA8 / PINCM19
#define LED_GREEN_PORT      GPIOA
#define LED_GREEN_PIN       DL_GPIO_PIN_8
#define LED_GREEN_IOMUX     IOMUX_PINCM19

// Yellow LED: PB24 / PINCM52
#define LED_YELLOW_PORT     GPIOB
#define LED_YELLOW_PIN      DL_GPIO_PIN_24
#define LED_YELLOW_IOMUX    IOMUX_PINCM52

// Red LED: PB9 / PINCM26
#define LED_RED_PORT        GPIOB
#define LED_RED_PIN         DL_GPIO_PIN_9
#define LED_RED_IOMUX       IOMUX_PINCM26

void led_init(void)
{
    // Green
    DL_GPIO_initDigitalOutput(LED_GREEN_IOMUX);
    DL_GPIO_clearPins(LED_GREEN_PORT, LED_GREEN_PIN);
    DL_GPIO_enableOutput(LED_GREEN_PORT, LED_GREEN_PIN);

    // Yellow
    DL_GPIO_initDigitalOutput(LED_YELLOW_IOMUX);
    DL_GPIO_clearPins(LED_YELLOW_PORT, LED_YELLOW_PIN);
    DL_GPIO_enableOutput(LED_YELLOW_PORT, LED_YELLOW_PIN);

    // Red
    DL_GPIO_initDigitalOutput(LED_RED_IOMUX);
    DL_GPIO_clearPins(LED_RED_PORT, LED_RED_PIN);
    DL_GPIO_enableOutput(LED_RED_PORT, LED_RED_PIN);
}

void led_set(led_id_t led, bool on)
{
    switch (led) {
        case LED_GREEN:
            if (on) DL_GPIO_setPins(LED_GREEN_PORT,  LED_GREEN_PIN);
            else    DL_GPIO_clearPins(LED_GREEN_PORT, LED_GREEN_PIN);
            break;
        case LED_YELLOW:
            if (on) DL_GPIO_setPins(LED_YELLOW_PORT,  LED_YELLOW_PIN);
            else    DL_GPIO_clearPins(LED_YELLOW_PORT, LED_YELLOW_PIN);
            break;
        case LED_RED:
            if (on) DL_GPIO_setPins(LED_RED_PORT,  LED_RED_PIN);
            else    DL_GPIO_clearPins(LED_RED_PORT, LED_RED_PIN);
            break;
        default:
            break;
    }
}

void led_all_off(void)
{
    led_set(LED_GREEN,  false);
    led_set(LED_YELLOW, false);
    led_set(LED_RED,    false);
}
