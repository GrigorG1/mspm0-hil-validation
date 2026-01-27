"""
HIL Loopback Validation Tests

These tests verify the firmware's GPIO loopback functionality.
Run with: pytest test_hil_loopback.py -v --port COM3
"""

import pytest
import time


class TestIdentity:
    """Tests for the '?' (identity) command."""
    
    def test_identity_returns_correct_version(self, client, logger):
        """
        Verify device responds with correct firmware identifier.
        
        This is typically the first test - if this fails, nothing else
        will work (wrong device, bad connection, etc.)
        """
        logger.info("TEST: Identity check")
        
        identity = client.get_identity()
        logger.info(f"  Response: {identity}")
        
        assert identity == "MSPM0_HIL_v1.0", f"Unexpected identity: {identity}"


class TestGPIOLoopback:
    """
    Tests for GPIO loopback functionality.
    
    These tests require the loopback wire connected:
    PB2 (output) -> PB3 (input)
    """
    
    def test_set_high_read_high(self, client, logger):
        """
        Set output HIGH, verify input reads HIGH.
        
        Expected signal path:
        H command -> PB2 goes HIGH -> wire -> PB3 reads HIGH
        """
        logger.info("TEST: Set HIGH, read HIGH")
        
        # Set output high
        success = client.set_pin_high()
        logger.info(f"  H command: {'OK' if success else 'FAIL'}")
        assert success, "Failed to set pin HIGH"
        
        # Read input
        value = client.read_pin()
        logger.info(f"  R command: {value}")
        assert value == 1, f"Expected 1, got {value} - check loopback wire"
    
    def test_set_low_read_low(self, client, logger):
        """
        Set output LOW, verify input reads LOW.
        
        Expected signal path:
        L command -> PB2 goes LOW -> wire -> PB3 reads LOW
        """
        logger.info("TEST: Set LOW, read LOW")
        
        # Set output low
        success = client.set_pin_low()
        logger.info(f"  L command: {'OK' if success else 'FAIL'}")
        assert success, "Failed to set pin LOW"
        
        # Read input
        value = client.read_pin()
        logger.info(f"  R command: {value}")
        assert value == 0, f"Expected 0, got {value}"
    
    def test_toggle_sequence(self, client, logger):
        """
        Rapidly toggle output and verify input follows.
        
        This catches timing issues or stuck pins.
        """
        logger.info("TEST: Toggle sequence (H-L-H-L)")
        
        expected_sequence = [1, 0, 1, 0]
        actual_sequence = []
        
        for expected in expected_sequence:
            if expected == 1:
                client.set_pin_high()
            else:
                client.set_pin_low()
            
            value = client.read_pin()
            actual_sequence.append(value)
            logger.info(f"  Set {expected}, read {value}")
        
        assert actual_sequence == expected_sequence, \
            f"Sequence mismatch: expected {expected_sequence}, got {actual_sequence}"


class TestStatus:
    """Tests for the 'S' (status) command."""
    
    def test_status_returns_uptime_and_count(self, client, logger):
        """Verify status command returns valid uptime and count."""
        logger.info("TEST: Status command")
        
        uptime, count = client.get_status()
        logger.info(f"  Uptime: {uptime}ms, Count: {count}")
        
        # Uptime should be positive (device has been running)
        assert uptime > 0, f"Uptime should be > 0, got {uptime}"
        
        # Count should be positive (we've sent commands already)
        assert count >= 1, f"Command count should be >= 1, got {count}"
    
    def test_uptime_increases(self, client, logger):
        """Verify uptime is actually incrementing."""
        logger.info("TEST: Uptime increasing")
        
        uptime1, _ = client.get_status()
        logger.info(f"  First reading: {uptime1}ms")
        
        # Wait a bit
        time.sleep(0.5)
        
        uptime2, _ = client.get_status()
        logger.info(f"  Second reading: {uptime2}ms (after 500ms delay)")
        
        # Should have increased by at least 400ms (allowing some tolerance)
        delta = uptime2 - uptime1
        logger.info(f"  Delta: {delta}ms")
        
        assert delta >= 400, f"Uptime delta {delta}ms is less than expected 400ms"


class TestErrorHandling:
    """Tests for error handling and edge cases."""
    
    def test_invalid_command_returns_error(self, client, logger):
        """Verify invalid commands return error response."""
        logger.info("TEST: Invalid command handling")
        
        response = client.send_command("X")
        logger.info(f"  Sent 'X', got: {response}")
        
        assert response.startswith("E"), f"Expected error response, got: {response}"
        assert "BAD_CMD" in response, f"Expected BAD_CMD error, got: {response}"
    
    def test_device_responds_after_error(self, client, logger):
        """
        Verify device still works after receiving invalid command.
        
        A robust parser shouldn't crash or hang on bad input.
        """
        logger.info("TEST: Recovery after error")
        
        # Send garbage
        client.send_command("Z")
        client.send_command("!")
        logger.info("  Sent invalid commands 'Z' and '!'")
        
        # Should still respond to valid command
        identity = client.get_identity()
        logger.info(f"  Identity after errors: {identity}")
        
        assert identity == "MSPM0_HIL_v1.0", "Device not responding after error"


class TestWireDisconnected:
    """
    Tests for fault detection (wire disconnected).
    
    NOTE: These tests require REMOVING the loopback wire.
    They are marked with 'manual' marker and skipped by default.
    Run with: pytest -v -m manual
    """
    
    @pytest.mark.manual
    def test_read_low_when_wire_disconnected(self, client, logger):
        """
        With wire disconnected, input should read LOW (pull-down).
        
        SETUP: Remove loopback wire before running this test.
        """
        logger.info("TEST: Read with wire disconnected")
        logger.warning("  MANUAL TEST - Ensure loopback wire is DISCONNECTED")
        
        # Set output HIGH
        client.set_pin_high()
        logger.info("  Set output HIGH")
        
        # Without wire, input should read LOW (pull-down resistor)
        value = client.read_pin()
        logger.info(f"  Read input: {value} (expected: 0)")
        
        assert value == 0, "Input should be LOW with wire disconnected (pull-down)"