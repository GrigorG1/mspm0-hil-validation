/*
 * ======== uart_io.c ========
 * Minimal non-blocking UART I/O on UART0 (PA10 TX / PA11 RX).
 *
 * Init sequence mirrors what the TI Drivers UARTMSP_enable() does (pin IOMUX,
 * clock config, FIFOs, baud, NVIC). Borrowed from the SDK reference
 * implementation at source/ti/drivers/uart/UARTMSPM0.c.
 *
 * Baud calculation: BUSCLK = 32 MHz, no divider, so the UART input clock is
 * 32 MHz. DL_UART_configBaudRate handles oversampling math internally.
 */

#include <stdint.h>
#include <stdbool.h>
// NOTE: include dl_uart_main.h (not dl_uart.h). dl_uart.h's body is guarded
// so it only activates when included through dl_uart_main.h or dl_uart_extend.h
// — otherwise none of the DL_UART_* types/constants are defined.
#include <ti/driverlib/dl_uart_main.h>
#include <ti/driverlib/dl_gpio.h>
#include "ti_drivers_config.h"   // for UART0, IOMUX_PINCM21/22, UART0_INT_IRQn
#include "uart_io.h"

// LaunchPad XDS110 back-channel pins on the MSPM0G3507
#define UART_IO_TX_PINCM      IOMUX_PINCM21
#define UART_IO_TX_FUNC       IOMUX_PINCM21_PF_UART0_TX
#define UART_IO_RX_PINCM      IOMUX_PINCM22
#define UART_IO_RX_FUNC       IOMUX_PINCM22_PF_UART0_RX

#define UART_IO_BAUD          (115200u)
#define UART_IO_CLK_HZ        (32000000u)   // BUSCLK, divide ratio = 1

// Single-byte RX slot written from ISR, read from main loop.
// Bytes arriving back-to-back before the main loop consumes are overwritten;
// acceptable for a REPL-style command interface driven by human keystrokes.
static volatile char g_rx_byte;
static volatile bool g_rx_ready = false;

void uart_io_init(void)
{
    // 1. Route PA10 / PA11 to the UART0 peripheral function.
    DL_GPIO_initPeripheralOutputFunction(UART_IO_TX_PINCM, UART_IO_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction (UART_IO_RX_PINCM, UART_IO_RX_FUNC);

    // 2. Reset and power the UART0 peripheral.
    DL_UART_reset(UART0);
    DL_UART_enablePower(UART0);

    // TRM: wait >= 4 ULPCLK cycles after PWREN before touching regs.
    volatile int i;
    for (i = 0; i < 1000; i++);

    // 3. Clock source: BUSCLK, no divide.
    DL_UART_ClockConfig clockConfig = {
        .clockSel    = DL_UART_CLOCK_BUSCLK,
        .divideRatio = DL_UART_CLOCK_DIVIDE_RATIO_1,
    };
    DL_UART_setClockConfig(UART0, &clockConfig);

    // 4. Frame format: normal mode, TX+RX, no flow control, 8N1.
    DL_UART_Config config = {
        .mode        = DL_UART_MODE_NORMAL,
        .direction   = DL_UART_DIRECTION_TX_RX,
        .flowControl = DL_UART_FLOW_CONTROL_NONE,
        .parity      = DL_UART_PARITY_NONE,
        .wordLength  = DL_UART_WORD_LENGTH_8_BITS,
        .stopBits    = DL_UART_STOP_BITS_ONE,
    };
    DL_UART_init(UART0, &config);
    DL_UART_configBaudRate(UART0, UART_IO_CLK_HZ, UART_IO_BAUD);

    // 5. FIFOs on. RX threshold = 1 entry so we get an interrupt per byte
    //    (minimum latency, fine at 115200 baud). TX threshold irrelevant
    //    here because we don't use TX interrupts.
    DL_UART_enableFIFOs(UART0);
    DL_UART_setRXFIFOThreshold(UART0, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_setTXFIFOThreshold(UART0, DL_UART_TX_FIFO_LEVEL_EMPTY);

    // 6. Enable RX interrupt in the peripheral and at NVIC.
    DL_UART_clearInterruptStatus(UART0, DL_UART_INTERRUPT_RX);
    DL_UART_enableInterrupt(UART0, DL_UART_INTERRUPT_RX);
    NVIC_EnableIRQ(UART0_INT_IRQn);

    // 7. Go.
    DL_UART_enable(UART0);
}

bool uart_io_try_read(char *c)
{
    if (!g_rx_ready) {
        return false;
    }
    *c = g_rx_byte;
    g_rx_ready = false;
    return true;
}

void uart_io_write(const char *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        while (DL_UART_isTXFIFOFull(UART0)) {
            // spin until the FIFO has room
        }
        DL_UART_transmitData(UART0, (uint8_t) buf[i]);
    }
}

// RX ISR. Drains the FIFO; only the newest byte is surfaced to the main loop.
void UART0_IRQHandler(void)
{
    while (!DL_UART_isRXFIFOEmpty(UART0)) {
        g_rx_byte  = (char) DL_UART_receiveData(UART0);
        g_rx_ready = true;
    }
    DL_UART_clearInterruptStatus(UART0, DL_UART_INTERRUPT_RX);
}
