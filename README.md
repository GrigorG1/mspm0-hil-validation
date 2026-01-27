# MSPM0 Hardware-in-the-Loop (HIL) Validation Framework

## Overview
This project implements a lightweight HIL testing framework for the TI MSPM0G3507 LaunchPad. It demonstrates automated hardware validation by pairing a firmware test agent (running on the MCU) with a Python-based host controller.

**Key Features:**
* **Bare-Metal Firmware:** Custom UART command parser written in C (based on the TI Drivers SDK).
* **Automated Testing:** Python script using `pyserial` to drive inputs and verify outputs.
* **Hardware Loopback:** Physically verifies GPIO logic levels using a closed-loop wiring setup.

## Hardware Setup
* **Board:** TI LP-MSPM0G3507
* **Interface:** XDS110 Application UART (baud: 115200, data size: 8, parity: none: N, stop bits: 1)
* **Wiring:** Connected J1 Pin 9 (PB2) to J1 Pin 10 (PB3).
    * **PB2:** Stimulus Output (driven by Firmware command).
    * **PB3:** Measurement Input (read by Firmware command).

## Serial Protocol (v1.0)
The host communicates with the firmware via UART.

| Command | Description | Response (Success) | Response (Fail) |
| :--- | :--- | :--- | :--- |
| `?` | **Identification:** verifies the device is present and running correct firmware. | `OK MSPM0_HIL_v1.0` | (No response) |
| `H` | **Set High:** Drives PB2 (Output) to Logic High (3.3V). | `OK` | `E <ERROR_CODE>` |
| `L` | **Set Low:** Drives PB2 (Output) to Logic Low (0V). | `OK` | `E <ERROR_CODE>` |
| `R` | **Read:** Reads the state of PB3 (Input). | `OK 1` (High) or `OK 0` (Low) | `E <ERROR_CODE>` |
| `S` | **Status:** Returns uptime (ms) and command count. | `OK <uptime> <count>` | `E <ERROR_CODE>` |


## Project Structure
* `/firmware`: CCS Theia project source code (C).
* `/tests`: Python test scripts for host-side automation.
* `/docs`: Documentation.