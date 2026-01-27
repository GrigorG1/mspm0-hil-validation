"""
Manual test script for HIL client.

Usage: python test_client_manual.py COM3
       (replace COM3 with your actual port)
"""

import sys
from hil_client import HILClientContext


def main():
    if len(sys.argv) < 2:
        print("Usage: python test_client_manual.py <COM_PORT>")
        print("Example: python test_client_manual.py COM3")
        sys.exit(1)
    
    port = sys.argv[1]
    print(f"Connecting to {port}...")
    
    with HILClientContext(port) as client:
        # Test identity
        identity = client.get_identity()
        print(f"Identity: {identity}")
        
        # Test GPIO loopback
        print("Setting pin HIGH...")
        client.set_pin_high()
        
        value = client.read_pin()
        print(f"Read pin: {value} (expected: 1)")
        
        print("Setting pin LOW...")
        client.set_pin_low()
        
        value = client.read_pin()
        print(f"Read pin: {value} (expected: 0)")
        
        # Test status
        uptime, count = client.get_status()
        print(f"Status: uptime={uptime}ms, cmd_count={count}")
        
        # Test invalid command
        print("Sending invalid command 'X'...")
        response = client.send_command("X")
        print(f"Response: {response} (expected: E BAD_CMD)")
    
    print("\nAll manual tests completed!")


if __name__ == "__main__":
    main()