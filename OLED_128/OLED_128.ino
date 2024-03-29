#define COAP_BUF_MAX_SIZE 256
#include <coap-simple.h>                    //coapsimplelibrary by      Hirotaka(or https://github.com/automote/ESP-CoAP)
#include <WiFiManager.h>                    //wifimanager       by      tzapu
#include <ArduinoJson.h>                    //arduinojson       by      benoit
#include <ArduinoOTA.h>
#include <WiFiUdp.h>


#if defined(ESP8266)

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "SH1106Wire.h"                     //esp8266 oled1306  by      thingpulse
SH1106Wire display(0x3c, 2 , 0);            //d1 d2
String clientId = String(ESP.getChipId());
#define mdns_update(...) MDNS.update()

#elif defined(ESP32)

#include <ESPmDNS.h>
#include "SH1106Wire.h"                     //esp32 oled1306  by      thingpulse
SH1106Wire display(0x3c, 22 , 21);
String clientId = String(ESP.getEfuseMac());
#define mdns_update(...)

#endif


#if defined(DEBUG_ESP_PORT) && defined(ESP8266)
#define DEBUG_MSG(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#elif defined(ESP32)
#define DEBUG_MSG(...) log_i( __VA_ARGS__ )
#else
#define DEBUG_MSG(...)
#endif

WiFiUDP udp;
Coap coap(udp);
WiFiManager wifiManager;
WiFiClient espClient;
//coapClient coap;
IPAddress ip(0,0,0,0);
uint16_t port = 5683;

const int HWver = 200814;                                             //Hardware version compared with a sub value
long long int mil = 0 , count = 20 , servercount = 0;
bool coap_connected = false;

const size_t capacity = JSON_OBJECT_SIZE(8) + 250;
DynamicJsonDocument doc(capacity);

void print(String payload)
{
  deserializeJson(doc, payload);
  int    bigmode  = doc["bigmode"];
  String gpu_name = doc["gpu_name"];
  String gpu_load = doc["gpu_load"];
  String gpu_temp = doc["gpu_temp"];
  String cpu_name = doc["cpu_name"];
  String cpu_load = doc["cpu_load"];
  String cpu_temp = doc["cpu_temp"];
  String mem_load = doc["mem_load"];
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(1, 11 , "CPU");
  display.drawString(1, 37 , "GPU");
  if(bigmode) display.setFont(ArialMT_Plain_24);
  display.drawString(42 - bigmode*5, 11 - bigmode*3 , cpu_temp);
  display.drawString(90 - bigmode*7, 11 - bigmode*3 , cpu_load);
  display.drawString(42 - bigmode*5, 37 - bigmode*3 , gpu_temp);
  display.drawString(90 - bigmode*7, 37 - bigmode*3 , gpu_load);
  display.setFont(ArialMT_Plain_10);
  if(!bigmode)
  {
    display.drawString(1 , 52 , "SYSRAM");
    display.drawString(56, 52 , mem_load);
    display.drawString(0 , 0  , cpu_name);
    display.drawString(0 , 26 , gpu_name);
    display.drawString(80, 52 , "GB");
  }
  display.drawString(72, 12 - bigmode*9 , "°C");
  display.drawString(72, 38 - bigmode*9 , "°C");
  display.drawString(120, 12 - bigmode*9 , "%");
  display.drawString(120, 38 - bigmode*9 , "%");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.display();
}

//void callback_response(coapPacket &packet, IPAddress ip, int port) {
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
    coap_connected = true;
    char payload[packet.payloadlen + 1];
    memcpy(payload, packet.payload, packet.payloadlen);
    payload[packet.payloadlen] = NULL;
    print(payload);
    DEBUG_MSG("%s", payload);
}

void update_started() {
  DEBUG_MSG("\t\t\t\t\t---OTA Update---");
  display.clear();
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
  display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Restart");
  display.display();
}

void setup() {
  Serial.begin(115200);
  ArduinoOTA.onStart(update_started);
  ArduinoOTA.onProgress(update_progress);
  ArduinoOTA.onEnd(update_finished);
 
  pinMode(4 , OUTPUT);
  pinMode(5 , OUTPUT);
  digitalWrite(4 , HIGH);
  digitalWrite(5 , LOW);
  delay(5);
  display.init();
  display.clear();
  display.setColor(WHITE);
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Connecting");
  display.display();
  
  WiFi.hostname("OLED Display");
  wifiManager.setTimeout(90);
  wifiManager.autoConnect("OLED Display");
  DEBUG_MSG("\t\t\t\t\t---Connected---");
  ArduinoOTA.setHostname("OLED Display");
  ArduinoOTA.begin();
  MDNS.begin("OLED Display");
  WiFi.mode(WIFI_STA);
  coap.response(callback_response);
  coap.start();
}

void loop()
{
  mdns_update();
  if(count >= 20)
  {
    display.clear();
    display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 8, "Searching");
    DEBUG_MSG("\t\t\t\t\t---Searching---");
    servercount = MDNS.queryService("coap", "udp"); // Send out query for esp tcp services
    for(int i = 0 ; i < servercount ; i++)
    {
        ip = MDNS.IP(i);
        port = MDNS.port(i);
        coap.get(ip,port,"hwmon");
        DEBUG_MSG("%s:%d", ip.toString().c_str(), port);
        display.clear();
        display.drawString(display.getWidth() / 2, display.getHeight() / 2, ip.toString());
        display.display();
        coap.loop();
        if(coap_connected)
        {
          DEBUG_MSG("\t\t\t\t\t---Found---");
          break;
        }
        delay(500);
        coap_connected = false;
    }
  }
  if (millis() - mil >= 300)
  {
    mil = millis();
    coap.get(ip,port,"hwmon");
//    coap.loop();
    if(!coap_connected)
      count++;
    else
      count = 0;
    coap_connected = false;
    display.setColor(BLACK);
    display.drawLine(107 , 64 , 128 , 64);
    display.setColor(WHITE);
    int tmpcnt = 0;
    if(count > 30)
      count = 20;
    for(int j = 1 ; j <= (count + 9)/10 ; j++)
      for(int i = 1 ; i <= 10 ; i++ , tmpcnt++)
        if(tmpcnt <= count)
          display.setPixel(107 + i*2 , 65 - j*2);
    display.display();
  }
  if (WiFi.status() != WL_CONNECTED)                                                  //restart
  {
    DEBUG_MSG("disconnected from wifi restarting!");
    ESP.restart();
  }
  ArduinoOTA.handle();
  coap.loop();
}
