#!/usr/bin/env python3
import socket
import re

FLEET = {
    "Lefty": "192.168.2.103",
    "Righty": "192.168.2.116"
}

PORT = 23
TIMEOUT = 3.0

print(f"\n{'Cabinet':<10} | {'IP Address':<15} | {'Uptime':<20} | {'Boot Reason'}")
print("-" * 80)

for name, ip in FLEET.items():
    try:
        # Connect and send 'uptime'
        s = socket.create_connection((ip, PORT), timeout=TIMEOUT)
        s.sendall(b"uptime\r\n")

        # Read the response (wait briefly for the ESP32 to reply)
        response_data = b""
        while b"Last boot reason" not in response_data:
            chunk = s.recv(1024)
            if not chunk:
                break
            response_data += chunk

        s.close()
        response = response_data.decode('utf-8', errors='ignore')

        # Parse using regex
        uptime_match = re.search(r'Uptime:\s*(.*?)\r?\n', response)
        reason_match = re.search(r'Last boot reason:\s*(.*?)\r?\n', response)

        uptime_str = uptime_match.group(1).strip() if uptime_match else "Unknown"
        reason_str = reason_match.group(1).strip() if reason_match else "Unknown"

        # Clean up the reason string if it has ANSI color codes
        reason_str = re.sub(r'\x1b\[.*?m', '', reason_str)

        print(f"{name:<10} | {ip:<15} | {uptime_str:<20} | {reason_str}")

    except socket.timeout:
        print(f"{name:<10} | {ip:<15} | {'TIMEOUT':<20} | Unreachable")
    except Exception as e:
        print(f"{name:<10} | {ip:<15} | {'ERROR':<20} | {str(e)}")

print()
