/*
 * ======== hil_firmware.c ========
 * Project 2: HIL Validation Framework (v1.0)
 * Manual Configuration (No SysConfig)
 * Bare-metal firmware for MSPM0 SDK 2.09+
 */

#include <stdint.h>
#include <stddef.h>
#include <ti/drivers/GPIO.h>
#include <ti/driverlib/dl_timerg.h>
#include "ti_drivers_config.h"

/*
 * ================ HIL HARDWARE DEFINITIONS ================
 * We are using Header J1 pins:
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

// Globals for Status Command
volatile uint32_t g_uptime_ms = 0;
uint32_t g_cmd_count = 0;

// Timer Clock Configuration: 32MHz BUSCLK / 32 (prescale) = 1MHz tick
const DL_TimerG_ClockConfig gCommonTimerClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale    = 31  // 32MHz / 32 = 1MHz tick
};

// TimerG0 Interrupt Handler (Runs every 1ms)
void TIMG0_IRQHandler(void)
{
    // Clear the interrupt status so it can fire again
    DL_TimerG_clearInterruptStatus(TIMG0, DL_TIMER_INTERRUPT_ZERO_EVENT);
    g_uptime_ms++;
}

// Minimal Helper: Converts uint32 to string, returns length
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

/*
 * ======== HIL_Hardware_Init ========
 * Manually configures PB2 and PB3 without SysConfig.
 */
void HIL_Hardware_Init(void)
{
    // 1. TIMG0 uses BUSCLK (32MHz) with prescale for 1ms tick.

    // 2. Enable Power to Port A (Required for UART)
    DL_GPIO_reset(GPIOA);
    DL_GPIO_enablePower(GPIOA);

    // 3. Enable Power to Port B (Required for HIL Pins)
    DL_GPIO_reset(GPIO_HIL_PORT);
    DL_GPIO_enablePower(GPIO_HIL_PORT);
    
    // Simple loop delay to let power stabilize
    volatile int i;
    for(i=0; i<1000; i++); 

    // 4. Configure PB2 as OUTPUT (Initial Value: LOW)
    DL_GPIO_initDigitalOutput(GPIO_HIL_OUT_IOMUX);
    DL_GPIO_clearPins(GPIO_HIL_PORT, GPIO_HIL_OUT_PIN);
    DL_GPIO_enableOutput(GPIO_HIL_PORT, GPIO_HIL_OUT_PIN);

    // 5. Configure PB3 as INPUT with PULL-DOWN resistor
    //    SDK 2.09 Signature: (IOMUX, Inversion, Resistor, Hysteresis, Wakeup)
    DL_GPIO_initDigitalInputFeatures(GPIO_HIL_IN_IOMUX,
                                     DL_GPIO_INVERSION_DISABLE,
                                     DL_GPIO_RESISTOR_PULL_DOWN,
                                     DL_GPIO_HYSTERESIS_DISABLE,
                                     DL_GPIO_WAKEUP_DISABLE);

    // 6. Configure TIMG0 for 1ms uptime counter
    DL_TimerG_reset(TIMG0);
    DL_TimerG_enablePower(TIMG0);
    
    // Wait for timer power to stabilize
    for(i=0; i<1000; i++);

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

/*
 * ======== mainThread ========
 */
int main(void)
{
    UART_Handle uart;
    UART_Params uartParams;
    const char  echoPrompt[] = "MSPM0_HIL_v1.0: Ready (Type H/L/R/S)\n";
    char        input;
    uint32_t    pinStatus;
    char        responseBuf[64];
    int         idx;
    
    // Variables for 4-argument SDK calls
    size_t      bytesRead;
    size_t      bytesWritten;

    /* Initialize Peripherals */
    HIL_Hardware_Init();
    
    /* Initialize UART Params */
    UART_Params_init(&uartParams);
    uartParams.baudRate = 115200; 

    uart = UART_open(CONFIG_UART_0, &uartParams);

    if (uart == NULL) {
        while (1); // Trap if UART fails
    }

    // Send startup message (exclude '\0')
    UART_write(uart, echoPrompt, sizeof(echoPrompt) - 1, &bytesWritten); 

    /* Main Loop */
    while (1) {
        // Read 1 character (Blocking)
        UART_read(uart, &input, 1, &bytesRead);

        if (bytesRead > 0) {
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
                
                UART_write(uart, responseBuf, idx, &bytesWritten);
            }
            else if (input == 'H') {
                // Set Output HIGH
                g_cmd_count++;
                DL_GPIO_setPins(GPIO_HIL_PORT, GPIO_HIL_OUT_PIN);
                UART_write(uart, "OK\n", 3, &bytesWritten);
            }
            else if (input == 'L') {
                g_cmd_count++;
                // Set Output LOW
                DL_GPIO_clearPins(GPIO_HIL_PORT, GPIO_HIL_OUT_PIN);
                UART_write(uart, "OK\n", 3, &bytesWritten);
            }
            else if (input == 'R') {
                // Read Input
                g_cmd_count++;
                pinStatus = DL_GPIO_readPins(GPIO_HIL_PORT, GPIO_HIL_IN_PIN);
                if (pinStatus > 0) {
                    UART_write(uart, "OK 1\n", 5, &bytesWritten);
                } else {
                    UART_write(uart, "OK 0\n", 5, &bytesWritten);
                }
            }
            else if (input == '?') {
                g_cmd_count++;
                UART_write(uart, "OK MSPM0_HIL_v1.0\n", 18, &bytesWritten);
            }
            else if (input == '\r' || input == '\n') {
                // Ignore newlines 
            }
            else {
                // Unknown Command
                g_cmd_count++;
                UART_write(uart, "E BAD_CMD\n", 10, &bytesWritten);
            }
        }
    }
}