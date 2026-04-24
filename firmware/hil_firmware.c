/*
 * ======== hil_firmware.c ========
 * HIL Validation Framework (v1.1) + Sensor Monitor extension.
 * Manual Configuration (No SysConfig). Bare-metal firmware for MSPM0 SDK 2.09+.
 *
 * v1.0 — single-character HIL command interface (H/L/R/S/?), GPIO loopback.
 * v1.1 — adds an 'M' mode-toggle command that puts the firmware into a
 *        sensor-monitor mode: samples an external potentiometer via ADC,
 *        drives a 3-LED indicator (green/yellow/red) based on thresholds,
 *        and prints telemetry over UART every SENSOR_PERIOD_MS. HIL commands
 *        still work in either mode. Non-blocking UART I/O in uart_io.c.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ti/driverlib/dl_timerg.h>
#include "ti_drivers_config.h"
#include "led.h"
#include "adc.h"
#include "uart_io.h"

/*
 * ================ HIL HARDWARE DEFINITIONS ================
 * Use Header J1 pins:
 * - PB2 (Pin 9)  -> OUTPUT (Stimulus)
 * - PB3 (Pin 10) -> INPUT  (Measurement)
 */

// Port B Definitions
#define GPIO_HIL_PORT           GPIOB

// Output Pin: PB2 (IOMUX Pin 15)
#define GPIO_HIL_OUT_PIN        DL_GPIO_PIN_2
#define GPIO_HIL_OUT_IOMUX      IOMUX_PINCM15

// Input Pin: PB3 (IOMUX Pin 16)
#define GPIO_HIL_IN_PIN         DL_GPIO_PIN_3
#define GPIO_HIL_IN_IOMUX       IOMUX_PINCM16

#define TIMER_LOAD_VALUE        (999) // 1MHz / 1000 = 1ms (after prescale)

/*
 * ================ SENSOR MONITOR MODE ================
 * Software-defined mode flag. UART I/O (see uart_io.c) is non-blocking, so
 * the main loop polls for characters and runs the sensor task concurrently.
 * HIL commands (H/L/R/S/?) still work in either mode.
 */
typedef enum {
    MODE_HIL = 0,
    MODE_SENSOR
} mode_t;

// Thresholds split the 12-bit ADC range (0..4095) into three equal bands.
#define SENSOR_LOW_THRESH   (1365)
#define SENSOR_HIGH_THRESH  (2730)
#define SENSOR_PERIOD_MS    (200)

// Globals for Status Command
volatile uint32_t g_uptime_ms = 0;
uint32_t g_cmd_count = 0;

// Sensor-monitor state
volatile mode_t g_mode = MODE_HIL;
uint32_t g_last_sample_ms = 0;

// Timer Clock Configuration: 32MHz BUSCLK / 32 (prescale) = 1MHz tick
const DL_TimerG_ClockConfig gCommonTimerClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale    = 31
};

// TimerG0 Interrupt Handler (Runs every 1ms)
void TIMG0_IRQHandler(void)
{
    DL_TimerG_clearInterruptStatus(TIMG0, DL_TIMER_INTERRUPT_ZERO_EVENT);
    g_uptime_ms++;
}

// Converts uint32 to string, returns length
// (Avoids overhead of stdio.h / sprintf)
int ultoa_simple(uint32_t value, char *buf) {
    char temp[12];
    int i = 0, len = 0;
    
    if (value == 0) {
        buf[0] = '0';
        return 1;
    }

    // Convert to reverse string
    while (value != 0) {
        temp[i++] = (value % 10) + '0';
        value /= 10;
    }

    // Reverse into buffer
    while (i > 0) {
        buf[len++] = temp[--i];
    }
    return len;
}

// Copies a null-terminated literal into buf at idx, returns new idx. Keeps the
// stdio-free style consistent with ultoa_simple.
static int append_str(char *buf, int idx, const char *s) {
    while (*s) {
        buf[idx++] = *s++;
    }
    return idx;
}

// Map raw 12-bit ADC reading to a status label.
static const char *sensor_status(uint16_t raw) {
    if (raw < SENSOR_LOW_THRESH)  return "NORMAL";
    if (raw < SENSOR_HIGH_THRESH) return "WARNING";
    return "ALERT";
}

// Drive exactly one LED based on the current reading.
static void sensor_update_leds(uint16_t raw) {
    led_all_off();
    if (raw < SENSOR_LOW_THRESH)       led_set(LED_GREEN,  true);
    else if (raw < SENSOR_HIGH_THRESH) led_set(LED_YELLOW, true);
    else                               led_set(LED_RED,    true);
}

/*
 * ======== HIL_Hardware_Init ========
 * Manually configures PB2 and PB3 without SysConfig.
 */
void HIL_Hardware_Init(void)
{
    // Enable Power to Port A (Required for UART)
    DL_GPIO_reset(GPIOA);
    DL_GPIO_enablePower(GPIOA);

    // Enable Power to Port B (Required for HIL Pins)
    DL_GPIO_reset(GPIO_HIL_PORT);
    DL_GPIO_enablePower(GPIO_HIL_PORT);

    // Configure TIMG0 for 1ms uptime counter
    DL_TimerG_reset(TIMG0);
    DL_TimerG_enablePower(TIMG0);
    
    // TRM: wait >= 4 ULPCLK cycles after PWREN before touching regs.
    volatile int i;
    for (i = 0; i < 1000; i++);

    // Configure PB2 as OUTPUT (Initial Value: LOW)
    DL_GPIO_initDigitalOutput(GPIO_HIL_OUT_IOMUX);
    DL_GPIO_clearPins(GPIO_HIL_PORT, GPIO_HIL_OUT_PIN);
    DL_GPIO_enableOutput(GPIO_HIL_PORT, GPIO_HIL_OUT_PIN);

    // Configure PB3 as INPUT with PULL-DOWN resistor
    DL_GPIO_initDigitalInputFeatures(GPIO_HIL_IN_IOMUX,
                                     DL_GPIO_INVERSION_DISABLE,
                                     DL_GPIO_RESISTOR_PULL_DOWN,
                                     DL_GPIO_HYSTERESIS_DISABLE,
                                     DL_GPIO_WAKEUP_DISABLE);

    DL_TimerG_setClockConfig(TIMG0,
        (DL_TimerG_ClockConfig *) &gCommonTimerClockConfig);
    
    DL_TimerG_setLoadValue(TIMG0, TIMER_LOAD_VALUE);
    DL_TimerG_enableInterrupt(TIMG0, DL_TIMER_INTERRUPT_ZERO_EVENT);
    
    // Configure and start: repeat mode, count down, enable
    TIMG0->COUNTERREGS.CTRCTL = GPTIMER_CTRCTL_REPEAT_REPEAT_1 
                                | GPTIMER_CTRCTL_CM_DOWN
                                | GPTIMER_CTRCTL_EN_ENABLED;

    // Enable the Interrupt in the NVIC (Processor Core)
    NVIC_EnableIRQ(TIMG0_INT_IRQn);

    // Enable global interrupts
    __enable_irq();
}

int main(void)
{
    const char  echoPrompt[] = "MSPM0_HIL_v1.1: Ready (H/L/R/S/?, M=mode toggle)\n";
    char        input;
    uint32_t    pinStatus;
    char        responseBuf[64];
    int         idx;

    // Initialize Peripherals
    HIL_Hardware_Init();
    led_init();
    adc_init();
    uart_io_init();

    // Send startup banner (exclude '\0').
    uart_io_write(echoPrompt, sizeof(echoPrompt) - 1);

    /* Main Super-Loop
     *
     * Runs two concurrent "tasks":
     *   1. Non-blocking UART poll — handle any single character that arrived.
     *   2. Sensor-mode periodic task — every SENSOR_PERIOD_MS, sample ADC,
     *      update the indicator LED, and print a telemetry line.
     *
     * Command handling works in both modes; 'M' toggles between them.
     */
    while (1) {
        // (1) Non-blocking RX check. uart_io_try_read returns immediately;
        // if a byte arrived since last check, it's returned here.
        if (uart_io_try_read(&input)) {
            if (input == 'S') {
                // Status Command: OK <uptime> <count>
                g_cmd_count++;
                idx = 0;
                responseBuf[idx++] = 'O';
                responseBuf[idx++] = 'K';
                responseBuf[idx++] = ' ';
                idx += ultoa_simple(g_uptime_ms, &responseBuf[idx]);
                responseBuf[idx++] = ' ';
                idx += ultoa_simple(g_cmd_count, &responseBuf[idx]);
                responseBuf[idx++] = '\n';

                uart_io_write(responseBuf, idx);
            }
            else if (input == 'H') {
                // Set Output HIGH
                g_cmd_count++;
                DL_GPIO_setPins(GPIO_HIL_PORT, GPIO_HIL_OUT_PIN);
                uart_io_write("OK\n", 3);
            }
            else if (input == 'L') {
                g_cmd_count++;
                // Set Output LOW
                DL_GPIO_clearPins(GPIO_HIL_PORT, GPIO_HIL_OUT_PIN);
                uart_io_write("OK\n", 3);
            }
            else if (input == 'R') {
                // Read Input
                g_cmd_count++;
                pinStatus = DL_GPIO_readPins(GPIO_HIL_PORT, GPIO_HIL_IN_PIN);
                if (pinStatus > 0) {
                    uart_io_write("OK 1\n", 5);
                } else {
                    uart_io_write("OK 0\n", 5);
                }
            }
            else if (input == '?') {
                g_cmd_count++;
                uart_io_write("OK MSPM0_HIL_v1.1\n", 18);
            }
            else if (input == 'M') {
                // Toggle between HIL and SENSOR mode.
                g_cmd_count++;
                if (g_mode == MODE_HIL) {
                    g_mode = MODE_SENSOR;
                    // Underflow is intentional: forces the (now - last) >=
                    // period check to fire on the very next iteration, so
                    // the user sees telemetry immediately.
                    g_last_sample_ms = g_uptime_ms - SENSOR_PERIOD_MS;
                    uart_io_write("OK SENSOR\n", 10);
                } else {
                    g_mode = MODE_HIL;
                    led_all_off(); // don't leave an indicator LED on in HIL mode
                    uart_io_write("OK HIL\n", 7);
                }
            }
            else if (input == '\r' || input == '\n') {
                // Ignore newlines
            }
            else {
                // Unknown Command
                g_cmd_count++;
                uart_io_write("E BAD_CMD\n", 10);
            }
        }

        // (2) Sensor-mode periodic task. Only runs in SENSOR mode. Uses
        // unsigned subtraction so wraparound of g_uptime_ms is handled
        // correctly (uint32_t at 1ms ticks wraps after ~49 days).
        if (g_mode == MODE_SENSOR &&
            (g_uptime_ms - g_last_sample_ms) >= SENSOR_PERIOD_MS) {

            g_last_sample_ms = g_uptime_ms;

            uint16_t raw = adc_read();
            sensor_update_leds(raw);

            // Format: "[<uptime>ms] ADC: <raw> | Status: <label>\n"
            idx = 0;
            responseBuf[idx++] = '[';
            idx += ultoa_simple(g_uptime_ms, &responseBuf[idx]);
            idx  = append_str(responseBuf, idx, "ms] ADC: ");
            idx += ultoa_simple(raw, &responseBuf[idx]);
            idx  = append_str(responseBuf, idx, " | Status: ");
            idx  = append_str(responseBuf, idx, sensor_status(raw));
            responseBuf[idx++] = '\n';

            uart_io_write(responseBuf, idx);
        }
    }
}