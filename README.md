# MSPM0 Hardware-in-the-Loop (HIL) Validation Framework

## Overview

This project implements a lightweight HIL testing framework for the TI MSPM0G3507 LaunchPad. It demonstrates automated hardware validation by pairing a firmware test agent (running on the MCU) with a Python-based host controller.

![20260130_004017222_iOS](https://github.com/user-attachments/assets/3343fe37-43bb-4755-8683-fd8a977f46e4)

**Key Features:**
* **Bare-Metal Firmware:** Custom UART command parser written in C (TI Drivers SDK, no SysConfig).
* **Automated Testing:** Python pytest suite using `pyserial` to drive inputs and verify outputs.
* **Hardware Loopback:** Physically verifies GPIO logic levels using a closed-loop wiring setup.
* **Test Artifacts:** JUnit XML reports and timestamped logs for CI/CD integration.

---

## Hardware Setup

### Requirements
* **Board:** TI LP-MSPM0G3507
* **USB Cable:** Micro-USB (included with LaunchPad)
* **Jumper Wire:** One female-to-female jumper wire for loopback

### Wiring Diagram
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

### Pin Configuration
| Pin | Function | Direction | Config |
|-----|----------|-----------|--------|
| PB2 (J1.9) | Stimulus | Output | Push-pull |
| PB3 (J1.10) | Measurement | Input | Pull-down enabled |

**Why pull-down?** If the loopback wire is disconnected, the input reads a deterministic LOW (0) instead of floating garbage. This enables fault detection.

---

## Serial Protocol (v1.0)

**UART Settings:** 115200 baud, 8 data bits, no parity, 1 stop bit (8N1)

### Command Format
- **Input:** Single character + newline (e.g., `H\n`)
- **Output:** `OK <payload>\n` on success, `E <code>\n` on failure

### Commands

| Cmd | Description | Success Response | Failure Response |
|-----|-------------|------------------|------------------|
| `?` | Get firmware identity | `OK MSPM0_HIL_v1.0` | - |
| `H` | Set PB2 HIGH (3.3V) | `OK` | - |
| `L` | Set PB2 LOW (0V) | `OK` | - |
| `R` | Read PB3 state | `OK 0` or `OK 1` | - |
| `S` | Get status | `OK <uptime_ms> <cmd_count>` | - |

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
4. Flash and debug
5. Open a serial terminal to verify: you should see `MSPM0_HIL_v1.0: Ready (Type H/L/R/S)`

**Build/flash without CCS (optional):**
- This repo does not include a standalone makefile; the supported build flow is CCS/Theia import.
- You can still flash the built `.out`/`.hex` as preferred .

### 2. Run the Tests

**Prerequisites:**
- Python 3.10+
- Virtual environment (recommended)

**Steps (in PowerShell 7.0+):**
```bash
cd tests
python -m venv venv

venv\Scripts\activate

pip install -r requirements.txt
```

**Run all tests:**
```bash
mkdir -p results
pytest test_hil_loopback.py --port COM7 --junitxml=results/test_results.xml
```
(Replace `COM7` with your actual serial port)

**Find your port:**
- Windows: Device Manager → Ports → "XDS110 Class Application/User UART"

---

## Test Cases

| Test | Description | Pass Criteria |
|------|-------------|---------------|
| `test_identity_returns_correct_version` | Verify firmware responds | Response = `MSPM0_HIL_v1.0` |
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

### Manual Verification

If tests fail, verify hardware manually:
1. Open a serial terminal (PuTTY, CCS Terminal, etc.)
2. Connect at 115200 baud
3. Type `?` → should see `OK MSPM0_HIL_v1.0`
4. Type `H` → should see `OK`
5. Type `R` → should see `OK 1` (with wire connected)

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
2026-01-27 14:41:54,368 | INFO     |   Response: MSPM0_HIL_v1.0
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
├── firmware/                  # CCS Theia project
│   ├── hil_firmware.c         # Main firmware source
│   ├── ti_drivers_config.*    # UART driver configuration
│   └── ...
├── tests/                     # Python test suite
│   ├── conftest.py            # Pytest fixtures
│   ├── hil_client.py          # Serial communication module
│   ├── test_hil_loopback.py   # Test cases
│   ├── pytest.ini             # Pytest configuration
│   ├── requirements.txt       # Python dependencies
│   └── sample_output/         # Example test artifacts
└── README.md
```

---

## Future Enhancements (v2.0+)

- [ ] Soak testing (12-hour continuous run)
- [ ] Fault injection (firmware-simulated delays/drops)
- [ ] Power cycle testing (USB relay integration)
- [ ] I²C sensor validation
- [ ] Logic analyzer trace capture

---

## License

MIT License - See LICENSE file for details.

---
