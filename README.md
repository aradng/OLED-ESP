# OLED-ESP
Wireless Hardware Monitor Display with CoAP and MDNS autofinder
![Image of OLED-ESP](imgs/renderings.png)
# Setting Up
  * MQTT broker(mosquitto)
  * HTTP server(apache)
  * any esp varient
  
i2c pins are prediefined as 2 = SDA , 0 = SCL and mqtt topics as "display" , "state" , "update"(static ip or ddns makes it easier if you dont have access to all devices a modified version was used for greenhouse temp/prec monitoring)
