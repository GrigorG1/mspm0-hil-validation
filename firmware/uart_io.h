/*
 * ======== uart_io.h ========
 * Minimal non-blocking UART I/O, direct DriverLib.
 *
 * Bypass the TI Drivers UART because it was easier to do (likely its MSPM0 
 * port can't do non-blocking reads in our config: UART_readTimeout(...,0) 
 * still pends on a WAIT_FOREVER semaphore inside UART_readBufferedMode, and
 * callback mode requires DMA which is disabled in ti_drivers_config.c. So we 
 * talk to the hardware directly via DL_UART_*. 
 * //TODO look into the details at a later time.
 *
 * RX: interrupt-driven, single-byte slot with a "ready" flag. If bytes arrive
 *     faster than the main loop consumes them, only the newest is kept. That's
 *     fine for human-typed command characters.
 * TX: blocking, FIFO-polled. No interrupts.
 */

#ifndef UART_IO_H
#define UART_IO_H

#include <stdbool.h>
#include <stddef.h>

// Initialize UART0 at 115200 8N1 on PA10 (TX) / PA11 (RX), enable RX interrupt.
// Caller must have already powered Port A (HIL_Hardware_Init does this).
void uart_io_init(void);

// Non-blocking: if a byte is available, write it to *c and return true.
// Otherwise returns false immediately.
bool uart_io_try_read(char *c);

// Blocking: push every byte through the TX FIFO. Spins while FIFO is full.
void uart_io_write(const char *buf, size_t len);

#endif /* UART_IO_H */
