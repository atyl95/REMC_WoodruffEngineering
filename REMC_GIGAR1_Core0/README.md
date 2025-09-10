My setup

# Server stuff
python -m venv venv
.\venv\Scripts\activate
pip install flask waitress
`ipconfig` look for IPv4 adress under ethernet adapter ethernet

.py file
MULTICAST_INTERFACE_IP = '192.168.1.10'  # Local IP to send/receive multicast (PC's IP, on subnet 255.255.255.0)
WEB_SERVER_HOST = '192.168.1.10'  # IP for the web dashboard.


# Arduino stuff
installed Time by Michael Margolis Arduino library `#include <TimeLib.h>

# Computer stuff
Step 1: Configure PC Ethernet Adapter
Open Network Settings:
Press Win + R, type ncpa.cpl, press Enter
OR: Settings → Network & Internet → Advanced network settings → Change adapter options
Find your Ethernet adapter (the one connected to Arduino)
Right-click → Properties
Select "Internet Protocol Version 4 (TCP/IPv4)" → Properties
Select "Use the following IP address" and enter:
IP address: 192.168.1.10
Subnet mask: 255.255.255.0
Default gateway: 192.168.1.1 (or leave blank)
Preferred DNS: 8.8.8.8
Alternate DNS: 8.8.4.4

ipconfig /release "Ethernet"

