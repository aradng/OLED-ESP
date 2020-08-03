#include <ArduinoJson.h>
#include "coap_client.h"
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include "SS1306Brzo.h"
#include <ESP8266httpUpdate.h>

SSD1306Brzo display(0x3c, 5, 4);    //d1 d2

const int HWver = 200801;                                             //Hardware version compared with a sub value
String clientId = String(ESP.getChipId());

const size_t capacity = JSON_OBJECT_SIZE(1) + 3*JSON_OBJECT_SIZE(3) + 110;
DynamicJsonDocument doc(capacity);

WiFiManager wifiManager;
coapClient coap;
IPAddress ip(0,0,0,0);
int port =5683;

void callback_response(coapPacket &packet, IPAddress ip, int port) {
    char payload[packet.payloadlen + 1];
    memcpy(payload, packet.payload, packet.payloadlen);
    payload[packet.payloadlen] = NULL;
    
    deserializeJson(doc, payload);
    JsonObject gpu = doc["gpu"];
    String gpu_name = gpu["name"];
    String gpu_load = gpu["load"];
    String gpu_temp = gpu["temp"];
    JsonObject cpu = doc["cpu"];
    String cpu_name = cpu["name"];
    String cpu_load = cpu["load"];
    String cpu_temp = cpu["temp"];
    String mem_load = doc["mem"]["load"];
    
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(Dialog_plain_16);
    display.drawString(1, 11 , "CPU");
    display.drawString(1, 37 , "GPU");
    display.drawString(42, 11 , cpu_temp);
    display.drawString(90, 11 , cpu_load);
    display.drawString(42, 37 , gpu_temp);
    display.drawString(90, 37 , gpu_load);
    display.setFont(ArialMT_Plain_10);
    display.drawString(1 , 52 , "SYSRAM");
    display.drawString(56, 52 , mem_load);
    display.drawString(0 , 0  , cpu_name);
    display.drawString(0 , 26 , gpu_name);
    display.drawString(80, 52 , "GB");
    display.drawString(72, 12 , "°C");
    display.drawString(72, 38 , "°C");
    display.drawString(120, 12 , "%");
    display.drawString(120, 38 , "%");
    display.display();
}
                                                                  //display update
void update_started() {
  display.clear();
  display.setColor(WHITE);
  display.setFont(Dialog_plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 20, "OTA Update");
  display.display();
}

void update_progress(int progress, int total) {
  display.drawProgressBar(4, 32, 120, 12, progress / (total / 100) );
  display.display();
}

void update_finished() {
  display.clear();
  display.setFont(Dialog_plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Restart");
  display.display();
}

void setup()                                                                    ///setup
{
  ESPhttpUpdate.onStart(update_started);
  ESPhttpUpdate.onEnd(update_finished);
  ESPhttpUpdate.onProgress(update_progress);
  ArduinoOTA.onStart(update_started);
  ArduinoOTA.onProgress(update_progress);
  ArduinoOTA.onEnd(update_finished);
  
  display.init();
  display.clear();
  display.setColor(WHITE);
  display.setFont(Dialog_plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Connecting");
  display.display();
  
  WiFi.hostname("OLED Display");
  wifiManager.setTimeout(180);
  wifiManager.autoConnect("OLED Display");
  ArduinoOTA.setHostname("OLED Display");
  ArduinoOTA.begin();
  MDNS.begin("hwmon client");
  coap.response(callback_response);
  coap.start();
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth() / 2, 16 , "Connected");
  display.drawString(display.getWidth() / 2, 32 , WiFi.localIP().toString());
}

long long int mil = 0 , count = 0;

void loop() 
{
  if(count >= 10)
  {
    MDNS.queryService("CoAP", "udp"); // Send out query for esp tcp services
    ip = MDNS.IP(0);
    port = MDNS.port(0);
  }
  if (millis() - mil >= 300) 
  {
    coap.get(ip,port,"hwmon");
    mil = millis();
    if(coap.loop())
      count = 0; 
    else  count++;
  }
  if (WiFi.status() != WL_CONNECTED)                                                  //restart
    ESP.restart();
  ArduinoOTA.handle();
}
