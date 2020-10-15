# OLED-ESP
Wireless Hardware Monitor Display with CoAP and mDNS support
![Image of OLED-ESP](imgs/renderings.png)
# Setting Up
  * flash OLED_128
  * connect to OLED Display and input your network credentials (same as server network)
  * install python 3 or higher and dependencies for the Server
```sh
pip3 install -r requirements.txt 
```

  * Run HWmon (you can setup as a service/scheduler task) or use precompiled executables
```sh
pythonw hwmon.py
```
