#!/usr/bin/env python3
"""
Simple NTP client to test the basic NTP server
"""

import socket
import struct
import time

def ntp_client_test(server='127.0.0.1', port=123):
    """Test the NTP server with a simple client"""
    
    # Create NTP request packet (48 bytes)
    # Version 4, Mode 3 (client)
    request = b'\x1b' + b'\x00' * 47
    
    try:
        # Create UDP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(5)  # 5 second timeout
        
        # Send request
        print(f"Sending NTP request to {server}:{port}")
        sock.sendto(request, (server, port))
        
        # Receive response
        response, addr = sock.recvfrom(1024)
        
        if len(response) >= 48:
            # Unpack the transmit timestamp (last 8 bytes)
            tx_timestamp = struct.unpack('!II', response[40:48])
            tx_seconds = tx_timestamp[0]
            tx_fraction = tx_timestamp[1]
            
            # Convert from NTP timestamp to Unix timestamp
            ntp_epoch_offset = 2208988800
            server_time = tx_seconds - ntp_epoch_offset + (tx_fraction / (2**32))
            local_time = time.time()
            
            print(f"Response received from {addr[0]}:{addr[1]}")
            print(f"Server time: {time.ctime(server_time)}")
            print(f"Local time:  {time.ctime(local_time)}")
            print(f"Difference:  {server_time - local_time:.6f} seconds")
            
            return True
        else:
            print("Invalid response received")
            return False
            
    except socket.timeout:
        print("Request timed out")
        return False
    except Exception as e:
        print(f"Error: {e}")
        return False
    finally:
        sock.close()

def main():
    print("=== Basic NTP Client Test ===")
    
    # Test standard port first
    if not ntp_client_test('127.0.0.1', 123):
        print("\nTrying alternative port 12300...")
        ntp_client_test('127.0.0.1', 12300)

if __name__ == "__main__":
    main()
