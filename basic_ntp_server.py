#!/usr/bin/env python3
"""
Basic NTP Server - No frills, just works
Listens on UDP port 123 and responds with current system time
"""

import socket
import struct
import time
import threading

class BasicNTPServer:
    def __init__(self, host='0.0.0.0', port=123):
        self.host = host
        self.port = port
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.running = False
        
    def ntp_timestamp(self):
        """Convert current time to NTP timestamp format"""
        # NTP epoch starts Jan 1, 1900, Unix epoch starts Jan 1, 1970
        # Difference is 70 years = 2208988800 seconds
        ntp_epoch_offset = 2208988800
        current_time = time.time()
        ntp_time = current_time + ntp_epoch_offset
        
        # Split into seconds and fractions
        seconds = int(ntp_time)
        fraction = int((ntp_time - seconds) * (2**32))
        
        return seconds, fraction
    
    def create_ntp_response(self, request_data):
        """Create basic NTP response packet"""
        # Unpack the request (first byte contains version and mode)
        li_vn_mode = struct.unpack('!B', request_data[:1])[0]
        
        # Extract version (bits 3-5) and set mode to 4 (server)
        version = (li_vn_mode >> 3) & 0x07
        response_mode = (li_vn_mode & 0xC0) | (version << 3) | 4
        
        # Get current NTP timestamp
        tx_seconds, tx_fraction = self.ntp_timestamp()
        
        # Basic NTP response packet (48 bytes)
        response = struct.pack('!B B B b 11I',
            response_mode,      # LI, VN, Mode
            1,                  # Stratum (primary server)
            0,                  # Poll interval
            0,                  # Precision
            0,                  # Root delay
            0,                  # Root dispersion
            0x4C4F434C,         # Reference ID ("LOCL")
            0, 0,               # Reference timestamp
            0, 0,               # Origin timestamp
            0, 0,               # Receive timestamp  
            tx_seconds,         # Transmit timestamp (seconds)
            tx_fraction         # Transmit timestamp (fraction)
        )
        
        return response
    
    def handle_request(self, data, addr):
        """Handle incoming NTP request"""
        print(f"Received request from {addr[0]}:{addr[1]}")
        try:
            if len(data) >= 48:  # Valid NTP packet size
                response = self.create_ntp_response(data)
                self.socket.sendto(response, addr)
                print(f"Served time to {addr[0]}:{addr[1]}")
            else:
                print(f"Invalid packet size from {addr[0]}:{addr[1]}")
        except Exception as e:
            print(f"Error handling request from {addr}: {e}")
    
    def start(self):
        """Start the NTP server"""
        try:
            self.socket.bind((self.host, self.port))
            self.running = True
            print(f"Basic NTP Server started on {self.host}:{self.port}")
            print("Press Ctrl+C to stop")
            
            while self.running:
                try:
                    data, addr = self.socket.recvfrom(1024)
                    # Handle each request in a separate thread for better responsiveness
                    thread = threading.Thread(target=self.handle_request, args=(data, addr))
                    thread.daemon = True
                    thread.start()
                except socket.error as e:
                    if self.running:
                        print(f"Socket error: {e}")
                        
        except PermissionError:
            print("Error: Permission denied. Try running as administrator or use a port > 1024")
        except Exception as e:
            print(f"Server error: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """Stop the NTP server"""
        self.running = False
        self.socket.close()
        print("NTP Server stopped")

def main():
    # Try port 123 first (standard NTP), fall back to 12300 if permission denied
    try:
        server = BasicNTPServer('0.0.0.0', 123)
        server.start()
    except PermissionError:
        print("Port 123 requires admin privileges. Using port 12300 instead...")
        server = BasicNTPServer('0.0.0.0', 12300)
        server.start()
    except KeyboardInterrupt:
        print("\nShutting down...")
    except Exception as e:
        print(f"Failed to start server: {e}")

if __name__ == "__main__":
    main()
