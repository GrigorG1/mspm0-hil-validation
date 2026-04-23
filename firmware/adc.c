/*
 * ======== adc.c ========
 * ADC driver: reads the potentiometer wiper on PA25 (ADC0 channel 2).
 *
 * Design notes:
 * - Uses ADC0, channel 2, 12-bit result, VDDA (~3.3V) as reference.
 * - Polling-based completion (no ISR). Simpler than interrupt-driven and
 *   sufficient for a 200 ms sampling cadence. If we ever move to timer-driven
 *   sampling at a higher rate, switching to interrupts is a small change.
 * - Pin PA25 does not need any IOMUX configuration: its default "unconnected"
 *   digital state is equivalent to analog mode, and the ADC peripheral routes
 *   channel 2 internally.
 */

#include <ti/driverlib/dl_adc12.h>
#include "adc.h"

// Potentiometer is on ADC0, input channel 2 (wired to PA25 inside the chip).
#define POT_ADC_INST       ADC0
#define POT_ADC_CHANNEL    DL_ADC12_INPUT_CHAN_2
#define POT_ADC_REF        DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define POT_ADC_MEM        DL_ADC12_MEM_IDX_0

// Sample time in ADC clock cycles. At ULPCLK/8 (~4 MHz), 500 cycles = ~125 us.
// Plenty for a low-impedance pot (<10 kOhm source).
#define POT_SAMPLE_TIME    (500)

// ADC clock: derive from ULPCLK (same domain as the CPU), /8, in the 24-32 MHz
// frequency-range bucket that DriverLib uses to pick timing constants.
static const DL_ADC12_ClockConfig gAdcClockConfig = {
    .clockSel    = DL_ADC12_CLOCK_ULPCLK,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_8,
    .freqRange   = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};

void adc_init(void)
{
    // Power-cycle and enable the ADC peripheral.
    DL_ADC12_reset(POT_ADC_INST);
    DL_ADC12_enablePower(POT_ADC_INST);

    // TRM: wait >= 4 ULPCLK cycles after power-enable before touching regs.
    // Matching the existing firmware's crude-delay style.
    volatile int i;
    for (i = 0; i < 1000; i++);

    // Clock source / divider / frequency-range hint.
    DL_ADC12_setClockConfig(POT_ADC_INST,
        (DL_ADC12_ClockConfig *) &gAdcClockConfig);

    // Configure conversion memory slot 0:
    //   - input channel 2 (PA25)
    //   - VDDA reference
    //   - sample-timer source = SCOMP0 (uses setSampleTime0 below)
    //   - no hardware averaging, no burn-out detection, no window compare
    //   - AUTO_NEXT trigger mode: after a software-triggered conversion,
    //     stop (we only have one mem slot configured, so "next" = done).
    DL_ADC12_configConversionMem(POT_ADC_INST, POT_ADC_MEM,
        POT_ADC_CHANNEL, POT_ADC_REF,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);

    DL_ADC12_setPowerDownMode(POT_ADC_INST, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(POT_ADC_INST, POT_SAMPLE_TIME);

    DL_ADC12_enableConversions(POT_ADC_INST);
}

uint16_t adc_read(void)
{
    // Clear any stale completion flag from a previous conversion.
    DL_ADC12_clearInterruptStatus(POT_ADC_INST,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);

    // Trigger a new single-shot conversion.
    DL_ADC12_startConversion(POT_ADC_INST);

    // Poll until the MEM0 result-loaded flag is raised.
    while (!(DL_ADC12_getRawInterruptStatus(POT_ADC_INST,
             DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED))) {
        // busy wait (short — ~125 us at 4 MHz ADC clock)
    }

    uint16_t result = DL_ADC12_getMemResult(POT_ADC_INST, POT_ADC_MEM);

    // Re-arm for the next call (AUTO_NEXT stops after one conversion).
    DL_ADC12_enableConversions(POT_ADC_INST);

    return result;
}
