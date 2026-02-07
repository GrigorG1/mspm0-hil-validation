"""
HIL Client Module
Handles serial communication with the MSPM0 HIL firmware.
"""

import serial
import time
from typing import Optional, Tuple


class HILClient:
    """
    Serial client for communicating with HIL firmware.
    
    Protocol:
        - Send: single character command + newline (e.g., "H\n")
        - Receive: "OK <payload>\n" on success, "E <error>\n" on failure
    """
    
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 2.0):
        """
        Initialize client with serial port settings.
        
        Args:
            port: Serial port name (e.g., "COM3" on Windows, "/dev/ttyACM0" on Linux)
            baudrate: Communication speed (must match firmware: 115200)
            timeout: Seconds to wait for response before giving up
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial: Optional[serial.Serial] = None
    
    def connect(self) -> None:
        """
        Open serial connection and flush any stale data.
        
        When the board resets, it sends a startup message so we clear it in order to 
        get the response we're expecting.
        """
        self.serial = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            timeout=self.timeout
        )
        # Give the board time to reset if it just powered on
        time.sleep(0.1)
        # Clear any buffered data (startup message, garbage, etc.)
        self.serial.reset_input_buffer()
    
    def disconnect(self) -> None:
        """Close serial connection."""
        if self.serial and self.serial.is_open:
            self.serial.close()
            self.serial = None
    
    def send_command(self, cmd: str) -> str:
        """
        Send a command and return the response.
        
        Args:
            cmd: Single character command (e.g., "H", "L", "R", "S", "?")
        
        Returns:
            Response string with newline stripped (e.g., "OK 1")
        
        Raises:
            RuntimeError: If not connected
            TimeoutError: If no response within timeout period
        """
        if not self.serial or not self.serial.is_open:
            raise RuntimeError("Not connected. Call connect() first.")
        
        # Clear any pending input before sending
        self.serial.reset_input_buffer()
        
        # Send command with newline terminator converted to bytes
        self.serial.write(f"{cmd}\n".encode())
        
        # Read response line until \n or timeout
        response = self.serial.readline()
        
        if not response:
            raise TimeoutError(f"No response to command '{cmd}' within {self.timeout}s")
        
        # Decode the response and trim
        return response.decode().strip()
    
    def parse_response(self, response: str) -> Tuple[bool, str]:
        """
        Parse response into success flag and payload.
        
        Args:
            response: Raw response string (e.g., "OK 1" or "E BAD_CMD")
        
        Returns:
            Tuple of (success: bool, payload: str)
            - ("OK 1") -> (True, "1")
            - ("OK MSPM0_HIL_v1.0") -> (True, "MSPM0_HIL_v1.0")
            - ("E BAD_CMD") -> (False, "BAD_CMD")
        """
        if response.startswith("OK"):
            payload = response[3:] if len(response) > 3 else ""
            return (True, payload)
        elif response.startswith("E"):
            payload = response[2:] if len(response) > 2 else ""
            return (False, payload)
        else:
            return (False, f"UNEXPECTED: {response}")
    
    # ============ High-Level Commands ============
    # These wrap send_command() for cleaner test code
    
    def get_identity(self) -> str:
        """Send '?' command, return identity string."""
        response = self.send_command("?")
        success, payload = self.parse_response(response)
        if not success:
            raise RuntimeError(f"Identity command failed: {payload}")
        return payload
    
    def set_pin_high(self) -> bool:
        """Send 'H' command, return True if successful."""
        response = self.send_command("H")
        success, _ = self.parse_response(response)
        return success
    
    def set_pin_low(self) -> bool:
        """Send 'L' command, return True if successful."""
        response = self.send_command("L")
        success, _ = self.parse_response(response)
        return success
    
    def read_pin(self) -> int:
        """Send 'R' command, return pin state (0 or 1)."""
        response = self.send_command("R")
        success, payload = self.parse_response(response)
        if not success:
            raise RuntimeError(f"Read command failed: {payload}")
        return int(payload)
    
    def get_status(self) -> Tuple[int, int]:
        """
        Send 'S' command, return (uptime_ms, cmd_count).
        
        Returns:
            Tuple of (uptime in milliseconds, command count since boot)
        """
        response = self.send_command("S")
        success, payload = self.parse_response(response)
        if not success:
            raise RuntimeError(f"Status command failed: {payload}")
        parts = payload.split()
        return (int(parts[0]), int(parts[1]))


# ============ Context Manager Support ============
# Allows using "with HILClient(...) as client:" syntax

class HILClientContext:
    """
    Context manager wrapper for HILClient.
    
    Usage:
        with HILClientContext("COM3") as client:
            client.set_pin_high()
            value = client.read_pin()
        # Connection automatically closed here
    """
    
    def __init__(self, port: str, **kwargs):
        self.client = HILClient(port, **kwargs)
    
    def __enter__(self) -> HILClient:
        self.client.connect()
        return self.client
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.client.disconnect()