/*
 * ======== led.h ========
 * LED driver for the 3-LED status indicator (green/yellow/red).
 * Public interface only. Implementation lives in led.c.
 */

#ifndef LED_H
#define LED_H

#include <stdbool.h>

typedef enum {
    LED_GREEN,
    LED_YELLOW,
    LED_RED,
    LED_COUNT
} led_id_t;

void led_init(void);
void led_set(led_id_t led, bool on);
void led_all_off(void);

#endif /* LED_H */
