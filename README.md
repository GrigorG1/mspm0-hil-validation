# MSPM0 Hardware-in-the-Loop (HIL) Validation Framework

## Overview

This project implements a lightweight HIL testing framework for the TI MSPM0G3507 LaunchPad. It demonstrates automated hardware validation by pairing a firmware test agent (running on the MCU) with a Python-based host controller.

As of v1.1, the firmware adds a **Sensor Monitor** mode that samples an external potentiometer, drives a 3-LED status indicator (breadboarded with current-limiting resistors), and streams telemetry over the same UART — a small demo of closed-loop sensor processing on top of the same framework.

![20260130_004017222_iOS](https://github.com/user-attachments/assets/3343fe37-43bb-4755-8683-fd8a977f46e4)

*Photo above shows the v1.0 HIL loopback setup (PB2 ↔ PB3 jumper only). Updated photos covering the v1.1 Sensor Monitor breadboard (pot + 3 LEDs) are pending — tracked in [Future Enhancements](#future-enhancements).*

**Key Features:**
* **Bare-Metal Firmware:** Custom UART command parser written in C, direct TI DriverLib (no SysConfig).
* **Automated Testing:** Python `pytest` suite using `pyserial` to drive inputs and verify outputs.
* **Hardware Loopback:** Physically verifies GPIO logic levels using a closed-loop wiring setup.
* **Sensor Monitor Extension (v1.1):** ADC-driven threshold detection with LED indicators and UART telemetry; HIL commands remain available while in Sensor Monitor mode.
* **Test Artifacts:** JUnit XML reports and timestamped logs for CI/CD integration.

---

## Hardware Setup

### Requirements

**HIL loopback (v1.0):**
* **Board:** TI LP-MSPM0G3507
* **USB Cable:** Micro-USB (included with LaunchPad)
* **Jumper wire:** 1× female-to-female, for the PB2 ↔ PB3 loopback

**Sensor Monitor (v1.1), additionally:**
* **Potentiometer:** 1× 10 kΩ linear
* **LEDs:** 1× green, 1× yellow, 1× red (standard 3 mm or 5 mm indicator LEDs, ~2.0–2.2 V Vf)
* **Resistors:** 3× 270 Ω (current-limiting; one per LED)
* **Breadboard:** half-size (~400 tie points, ≥30 columns) or larger
* **Jumper wires:** ~10 female-to-female (one female end plugs directly onto the LaunchPad's male header pins; the other female end is converted to a pin for the breadboard via a short male-male pin-header adapter) and ~13 male-to-male (breadboard-to-breadboard hops)

### Wiring Diagrams

**HIL loopback (v1.0):**
```
    LP-MSPM0G3507 (Header J1)
    ┌─────────────────────────┐
    │                         │
    │   Pin 9  (PB2) ────┐    │
    │                    │    │   <- Jumper Wire
    │   Pin 10 (PB3) <───┘    │
    │                         │
    └─────────────────────────┘

    PB2 = Stimulus Output (firmware drives HIGH/LOW)
    PB3 = Measurement Input (firmware reads state)
```

**Sensor Monitor (v1.1):**
```
Voltage divider — potentiometer to ADC:

    J1.1  (3V3) ───┐
                   │
                 [ 10 kΩ pot ]── wiper ──▶ J1.2  (PA25, ADC0.2)
                   │
    J3.22 (GND) ───┘

LED chains — one per color, cathodes tied to the breadboard GND rail
(which is strapped to J3.22):

    J1.4 (PA8)  ── 270 Ω ──▶|── GND      Green  ("NORMAL")
    J1.6 (PB24) ── 270 Ω ──▶|── GND      Yellow ("WARNING")
    J1.7 (PB9)  ── 270 Ω ──▶|── GND      Red    ("ALERT")

Legend:  ▶|  = LED (anode left, cathode right)
         Active-high: GPIO HIGH → LED on.
```

### Pin Configuration

**HIL loopback (v1.0):**

| Pin | Function | Direction | Config |
|-----|----------|-----------|--------|
| PB2 (J1.9) | Stimulus | Output | Push-pull |
| PB3 (J1.10) | Measurement | Input | Pull-down* enabled |

*If the loopback wire is disconnected, the input reads a deterministic LOW (0) instead of floating garbage. This enables fault detection.

**Sensor Monitor extension (v1.1):**

| Pin  | Header | Function | Direction | Config |
|------|--------|----------|-----------|--------|
| PA25 | J1.2  | Potentiometer wiper | Analog input | ADC0 channel 2, VDDA reference |
| PA8  | J1.4  | Green LED ("NORMAL")  | Output | Push-pull, active high (270 Ω → GND) |
| PB24 | J1.6  | Yellow LED ("WARNING") | Output | Push-pull, active high (270 Ω → GND) |
| PB9  | J1.7  | Red LED ("ALERT")      | Output | Push-pull, active high (270 Ω → GND) |
| 3V3  | J1.1  | Pot supply | — | Powers the voltage divider |
| GND  | J3.22 | Common ground | — | LED cathode returns + pot low end |

The potentiometer forms a voltage divider between the 3.3 V rail and GND; its wiper returns the analog sense voltage to PA25. Each LED is driven directly by its GPIO through a 270 Ω current-limiting resistor to the common GND rail (active-high: GPIO HIGH turns the LED on).

---

## Serial Protocol (v1.1)

**UART Settings:** 115200 baud, 8 data bits, no parity, 1 stop bit (8N1)

### Command Format
- **Input:** Single character (newline optional; `\r` and `\n` are ignored)
- **Output:** `OK <payload>\n` on success, `E <code>\n` on failure

### Commands

| Cmd | Description | Success Response |
|-----|-------------|------------------|
| `?` | Get firmware identity | `OK MSPM0_HIL_v1.1` |
| `H` | Set PB2 HIGH (3.3V) | `OK` |
| `L` | Set PB2 LOW (0V) | `OK` |
| `R` | Read PB3 state | `OK 0` or `OK 1` |
| `S` | Get status | `OK <uptime_ms> <cmd_count>` |
| `M` | Toggle Sensor Monitor mode | `OK SENSOR` or `OK HIL` |

Unknown characters produce `E BAD_CMD\n`. HIL commands work in both HIL and Sensor Monitor modes.

---

## Sensor Monitor Mode (v1.1)

Sensor Monitor turns the LaunchPad into a simple closed-loop sensor demo. Enter by sending `M`; send `M` again to return to HIL mode.

**What it does**
1. Samples the external potentiometer (PA25, ADC0 ch2) every 200 ms.
2. Maps the 12-bit reading into one of three bands and drives the matching LED:

| Band | Raw range | LED | Status label |
|------|-----------|-----|--------------|
| Normal  | `0 – 1364`    | Green (PA8)   | `NORMAL` |
| Warning | `1365 – 2729` | Yellow (PB24) | `WARNING` |
| Alert   | `2730 – 4095` | Red (PB9)     | `ALERT` |

3. Emits one telemetry line per sample over UART:

```
[<uptime_ms>ms] ADC: <raw_0_4095> | Status: NORMAL|WARNING|ALERT
```

Example transcript:
```
OK SENSOR
[12345ms] ADC: 812 | Status: NORMAL
[12545ms] ADC: 2011 | Status: WARNING
[12745ms] ADC: 3550 | Status: ALERT
```

HIL commands (`H`/`L`/`R`/`S`/`?`) still work while Sensor Monitor is active — they interleave with telemetry lines because the main loop polls UART non-blockingly between samples.

### Architecture notes

* **Super-loop + two ISRs.** The main loop runs two cooperative "tasks": a non-blocking UART poll and a periodic ADC/LED/telemetry step. TIMG0 provides a 1 ms tick (`g_uptime_ms`) for scheduling; the UART RX ISR latches incoming bytes for the main loop to consume.
* **Why the firmware bypasses the TI Drivers UART.** The SDK's `UART_readTimeout(..., 0)` did not appear to return immediately on MSPM0 in this configuration — it looked like it was pending on a `WAIT_FOREVER` semaphore inside `UART_readBufferedMode`, which breaks the super-loop pattern. Callback mode would likely fix that but requires DMA, which is disabled in `ti_drivers_config.c`. `uart_io.c` therefore talks to the UART directly via DriverLib (`DL_UART_*`) with its own interrupt handler and a single-byte RX slot — tiny, deterministic, non-blocking.
  > **Caveat:** This diagnosis is based on reading the SDK source and symptom matching, not a deep instrumented trace, and the workaround was chosen under time pressure for the demo. Before building more on top of it, the root cause should be re-verified (e.g., step through `UART_readBufferedMode` with the debugger to confirm the `WAIT_FOREVER` path is actually entered) and the alternatives (enable DMA + callback mode, tweak the driver config, or open a TI E2E thread) re-evaluated.

---

## Getting Started

### 1. Flash the Firmware

**Prerequisites: (other versions may be suitable as well, however these are the only tested versions)**
- [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO) CCS Theia 20.04.0
- [MSPM0 SDK](https://www.ti.com/tool/MSPM0-SDK) 2.09.00.01

**Steps:**
1. Open CCS and import the project from `firmware/`
2. Connect the LaunchPad via USB
3. Build the project
4. Flash
5. Open a serial terminal to verify: you should see `MSPM0_HIL_v1.1: Ready (H/L/R/S/?, M=mode toggle)`

**Build/flash without CCS (optional):**
- This repo does not include a standalone makefile; the supported build flow is CCS/Theia import.

### 2. Run the Tests

**Prerequisites:**
- Python 3.10+
- Virtual environment (recommended)

**Steps (in PowerShell 7.0+):**
```powershell
cd tests
python -m venv venv

venv\Scripts\Activate.ps1

pip install -r requirements.txt
```
> If PowerShell blocks `Activate.ps1` with an execution-policy error, run it once in your shell: `Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned`.

**Run all tests:**
```powershell
New-Item -ItemType Directory -Force -Path results | Out-Null
pytest test_hil_loopback.py --port COM7 --junitxml=results/test_results.xml
```
(Replace `COM7` with your actual serial port)

**Find your port:**
- Windows: Device Manager → Ports → "XDS110 Class Application/User UART"

---

## Test Cases

| Test | Description | Pass Criteria |
|------|-------------|---------------|
| `test_identity_returns_correct_version` | Verify firmware responds | Response = `MSPM0_HIL_v1.1` |
| `test_set_high_read_high` | Loopback HIGH | H → R returns 1 |
| `test_set_low_read_low` | Loopback LOW | L → R returns 0 |
| `test_toggle_sequence` | Rapid toggle | H-L-H-L sequence reads 1-0-1-0 |
| `test_status_returns_uptime_and_count` | Status valid | uptime > 0, count ≥ 1 |
| `test_uptime_increases` | Timer running | uptime increases over 500ms |
| `test_invalid_command_returns_error` | Error handling | `X` returns `E BAD_CMD` |
| `test_device_responds_after_error` | Recovery | Valid command works after error |

---

## Troubleshooting

### Common Failures

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| `TimeoutError: No response` | Wrong COM port | Check Device Manager, update `--port` |
| `TimeoutError: No response` | CCS terminal still open | Close CCS serial console |
| `test_set_high_read_high` fails with `OK 0` | Loopback wire disconnected | Connect PB2 → PB3 |
| `test_set_low_read_low` fails with `OK 1` | Wrong pins wired | Verify J1.9 → J1.10 |
| All tests fail | Firmware not flashed | Rebuild and flash firmware |
| `E BAD_CMD` on valid command | Baud rate mismatch | Ensure 115200 baud |
| pytest shows `UNEXPECTED: [<ms>] ADC: ...` lines | Firmware is still in Sensor Monitor mode from a prior interactive session | Press the LaunchPad reset button before running pytest (tracked as a known issue in [Future Enhancements](#future-enhancements)) |
| No LEDs light in Sensor Monitor mode | LED polarity reversed or 270 Ω resistor missing / miswired | Verify: GPIO pin → 270 Ω → LED anode → LED cathode → GND rail |
| ADC stuck at `0` or `4095` regardless of pot position | Pot `3V3` / `GND` ends swapped, or wiper not connected to PA25 | Verify J1.1 (3V3) on one outer terminal, J3.22 (GND) on the other, wiper → J1.2 |

### Manual Verification

If tests fail, verify hardware manually:
1. Open a serial terminal (PuTTY, CCS Terminal, etc.)
2. Connect at 115200 baud
3. Type `?` → should see `OK MSPM0_HIL_v1.1`
4. Type `H` → should see `OK`
5. Type `R` → should see `OK 1` (with wire connected)
6. Type `M` → should see `OK SENSOR`, followed by a telemetry line every 200 ms (see [Sensor Monitor Mode](#sensor-monitor-mode-v11)). Type `M` again to return to HIL mode.

---

## Sample Output

### Pytest Console
```
$ pytest test_hil_loopback.py --port COM7
========================= test session starts =========================
collected 9 items / 1 deselected / 8 selected

test_hil_loopback.py::TestIdentity::test_identity_returns_correct_version PASSED
test_hil_loopback.py::TestGPIOLoopback::test_set_high_read_high PASSED
test_hil_loopback.py::TestGPIOLoopback::test_set_low_read_low PASSED
test_hil_loopback.py::TestGPIOLoopback::test_toggle_sequence PASSED
test_hil_loopback.py::TestStatus::test_status_returns_uptime_and_count PASSED
test_hil_loopback.py::TestStatus::test_uptime_increases PASSED
test_hil_loopback.py::TestErrorHandling::test_invalid_command_returns_error PASSED
test_hil_loopback.py::TestErrorHandling::test_device_responds_after_error PASSED

========================= 8 passed, 1 deselected in 1.44s =========================
```

### Log File (excerpt)
```
2026-01-27 14:41:54,264 | INFO     | Logging to: logs\hil_test_20260127_144154.log
2026-01-27 14:41:54,264 | INFO     | Connecting to COM7...
2026-01-27 14:41:54,365 | INFO     | Connected successfully
2026-01-27 14:41:54,366 | INFO     | TEST: Identity check
2026-01-27 14:41:54,368 | INFO     |   Response: MSPM0_HIL_v1.1
2026-01-27 14:41:54,370 | INFO     | Disconnecting...
2026-01-27 14:41:54,371 | INFO     | Connecting to COM7...
2026-01-27 14:41:54,474 | INFO     | Connected successfully
2026-01-27 14:41:54,474 | INFO     | TEST: Set HIGH, read HIGH
2026-01-27 14:41:54,484 | INFO     |   H command: OK
2026-01-27 14:41:54,485 | INFO     |   R command: 1
2026-01-27 14:41:54,486 | INFO     | Disconnecting...
```

See `tests/sample_output/` for complete example files.

---

## Project Structure
```
mspm0_hil_validation/
├── firmware/                          # CCS Theia project (bare-metal, no SysConfig)
│   ├── hil_firmware.c                 # Main: command parser, mode toggle, super-loop
│   ├── uart_io.c / uart_io.h          # Non-blocking UART I/O (direct DriverLib; bypasses TI Drivers UART)
│   ├── adc.c / adc.h                  # ADC driver (potentiometer on PA25, ADC0 ch2)
│   ├── led.c / led.h                  # 3-LED indicator driver (green/yellow/red)
│   ├── ti_drivers_config.c / .h       # TI Drivers config (v1.0 scaffolding; UART handler intentionally dropped — see note in .c)
│   ├── mspm0g3507.cmd                 # Linker command file
│   ├── startup_mspm0g350x_ticlang.c   # Cortex-M reset vector + startup
│   └── targetConfigs/                 # CCS debugger target configuration
├── tests/                             # Python test suite
│   ├── conftest.py                    # Pytest fixtures (serial port, logging)
│   ├── hil_client.py                  # Serial client wrapping the command protocol
│   ├── test_hil_loopback.py           # Automated HIL loopback tests
│   ├── test_client_manual.py          # Manual/exploratory client for interactive smoke-testing
│   ├── pytest.ini                     # Pytest configuration
│   ├── requirements.txt               # Python dependencies
│   └── sample_output/                 # Example JUnit XML + log from a real run
├── LICENSE
└── README.md
```

---

## Future Enhancements
- [ ] `S` response should include the current mode (`HIL` or `SENSOR`)
- [ ] Add photos of the v1.1 Sensor Monitor breadboard (pot voltage divider + 3 LED chains) alongside the existing v1.0 loopback photo
- [ ] Add test cases for the 1.1 changes
- [ ] Soak testing (12-hour continuous run)
- [ ] Fault injection (firmware-simulated delays/drops)
- [ ] Power cycle testing (USB relay integration)
- [ ] I²C sensor validation
- [ ] Logic analyzer trace capture
- [ ] **Auto-return to HIL mode on pytest connect.** `g_mode` persists across serial disconnects, so if the firmware is left in Sensor Monitor mode from a prior interactive session, the 200 ms telemetry lines can race ahead of `OK`/`E` responses in the serial buffer and cause intermittent `UNEXPECTED:` parse failures. Proposed fix in `tests/hil_client.py`: on `connect()`, query mode (e.g., add a dedicated query command, or inspect the response to a probe `M` and toggle back if needed) so the test session always starts in a known HIL state. Until then, press the board's reset button before running pytest if you've been using `M` interactively.

---

## License

MIT License - See LICENSE file for details.

---
