/*
  MQTT topidcs
   display
     text     : OUTPUT for display
     bitmap   : display bitmaps
     state    : 0(LCD_OFF) / 1(LCD_ON)
     mode     : 1(hardware monitor) / 2(display mode) / 3(reset)
   update   : channel sync for hardware version
*/
#include <base64.hpp>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include "SSD1306Brzo.h"
#include <ESP8266httpUpdate.h>

SSD1306Brzo display(0x3c, D1, D2);

const int HWver = 200420;                      //Hardware version compared with a sub value
String clientId = String(ESP.getChipId());
int failcount, mode = 0;
const char *mqtt_server = "192.168.1.3";
const char *local_server = "arad-pc.local";      //MQTT server address(add A/CNAME in hosts + ipconfig /flushdns
String cpuv , gput , cput , cpul , gpul , gpun , cpun , ram;

WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);

String conv(byte* s , int l)
{
  String ret;
  for (int i = 0 ; i < l ; i++)
    ret += char(s[i]);
  return ret;
}
void callback(char* topic, byte* payload, unsigned int length)
{
  String T = topic;
  
  if (T == "CPU/Voltage")
    cpuv = conv(payload, length);
  else if (T == "CPU/Temperature")
    cput = conv(payload, length);
  else if (T == "GPU/Temperature")
    gput = conv(payload, length);
  else if (T == "GPU/Load")
    gpul = conv(payload, length);
  else if (T == "CPU/Load")
    cpul = conv(payload, length);
  else if (T == "Mem/Load")
    ram = conv(payload, length);
  else if (T == "CPU/Name")
    cpun = conv(payload, length);
  else if (T == "GPU/Name")
    gpun = conv(payload, length);
    
  else if (T == "display/text")
  {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(Dialog_plain_8);
    display.drawString(0 , 0 , conv(payload , length));
    display.display();
  }
  
  else if (T == "display/brightness")
  {
    int br = 0;
    for(int i = 0 ; i < length ; i++)
    {
      br += char(payload[i]) - '0';
      br *= 10;
    }
    br /= 10;
    display.setBrightness(br);
  }
  
  else if (T == "display/state")
  {
    if ((char)payload[0] == '0')
      display.displayOff();
    else if ((char)payload[0] == '1')
      display.displayOn();
  }
  
  else if (T == "display/mode")
    mode = char(payload[0]) - '0';
  
  else if (T == "display/bitmap")
  {
    unsigned char out[1024];
    decode_base64(payload, out);
    display.clear();
    display.drawXbm(0, 0, 128, 64, out);
    display.display();
  }
  
  else if (T == "update")
  {
    int hw = 0, pow = 100000;
    for (int i = 0 ; i < 6 ; i++)
    {
      hw += (payload[i] - '0') * pow;
      pow /= 10;
    }
    if (HWver < hw)
    {
      WiFiClient localClient;
      ESPhttpUpdate.update(localClient, "http://192.168.1.3:8080/Arad/OLED-ESP/OLED_128/OLED_128.ino.nodemcu.bin");
      WiFiClient netClient;
      ESPhttpUpdate.update(netClient, "http://oled.aradng.ml/OLED_128/OLED_128.ino.nodemcu.bin");
    }
  }
}

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

void setup()
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
  wifiManager.setTimeout(180);
  wifiManager.autoConnect("SmartConfig");
  ArduinoOTA.begin();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth() / 2, 16 , "Connected");
  display.drawString(display.getWidth() / 2, 32 , WiFi.localIP().toString());
  display.setFont(Dialog_plain_16);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void reconnect()
{
  String msg;
  display.setFont(Dialog_plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth() / 2 , 48 , "MQTT   -");
  display.display();
  while (!client.connected())
  {
    msg = "ESP-" + clientId + " joined.";
    if (client.connect(msg.c_str()))
    {
      display.drawString(display.getWidth() / 2  + 32 , 48 , "+");
      display.display();
      client.subscribe("display/text");
      client.subscribe("display/state");
      client.subscribe("display/mode");
      client.subscribe("display/brightness");
      client.subscribe("display/bitmap");
      client.subscribe("CPU/Voltage");
      client.subscribe("CPU/Temperature");
      client.subscribe("GPU/Temperature");
      client.subscribe("CPU/Load");
      client.subscribe("GPU/Load");
      client.subscribe("CPU/Name");
      client.subscribe("GPU/Name");
      client.subscribe("Mem/Load");
      client.subscribe("update");
      client.publish("ack", msg.c_str());
    }
    else
    {
      failcount++;
      if (failcount % 2)
        client.setServer(local_server, 1883);
      else
        client.setServer(mqtt_server, 1883);
      display.setColor(BLACK);
      display.drawString(display.getWidth() / 2  + 32 , 48 , "+");
      display.setColor(WHITE);
      display.drawString(display.getWidth() / 2  + 32 , 48 , "-");
      display.display();
    }
  }
}

long long int mil = 0;
int i = 0;

void loop() {
  ArduinoOTA.handle();
  if (mode == 1 && (millis() - mil) >= 500)                                 //hwmonitor
  {
    mil = millis();
    hwmonitor();
  }
  if (mode == 2)                                                            //displaymode
  {
    i++;
    for (int j = 0 ; j < display.getWidth(); j++)
      for (int k = 0 ; k < display.getHeight() ; k++)
      {
        if (((j + i) / 32 + k / 32) % 2)
          display.setColor(BLACK);
        else display.setColor(WHITE);
        display.setPixel(j, k);
      }
    if (i >= 32 * 2)
      i = 0;
    display.display();
  }
  else display.setColor(WHITE);
  if (WiFi.status() != WL_CONNECTED || mode == 3)
    ESP.restart();
  if (!client.connected() && WiFi.status() == WL_CONNECTED)
    reconnect();
  client.loop();
}

void hwmonitor()
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(Dialog_plain_16);
  display.drawString(1, 11 , "CPU");
  display.drawString(1, 37 , "GPU");
  display.drawString(42, 11 , cput);
  display.drawString(90, 11 , cpul);
  display.drawString(42, 37 , gput);
  display.drawString(90, 37 , gpul);
  display.setFont(ArialMT_Plain_10);
  display.drawString(1 , 52 , "SYSRAM");
  display.drawString(56, 52 , ram);
  display.drawString(0 , 0  , cpun);
  display.drawString(0 , 26 , gpun);
  display.drawString(76, 52 , "GB");
  display.drawString(72, 12 , "°C");
  display.drawString(72, 38 , "°C");
  display.drawString(120, 12 , "%");
  display.drawString(120, 38 , "%");
  display.display();
}
