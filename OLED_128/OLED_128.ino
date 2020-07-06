/*
  MQTT topidcs
   display
     text     : OUTPUT for display
     bitmap   : display bitmaps
     state    : 0(LCD_OFF) / 1(LCD_ON)
     mode     : 1(hardware monitor) / 2(display mode) / 3(reset)
   update   : channel sync for hardware version
*/
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include "SSD1306Brzo.h"
#include <ESP8266httpUpdate.h>

SSD1306Brzo display(0x3c, 5, 4);    //d1 d2

const int HWver = 200705;                                             //Hardware version compared with a sub value
String clientId = String(ESP.getChipId());
const char *mqtt_server = "192.168.1.3";
const char *local_server = "arad-pc.local";                           //MQTT server address(add A/CNAME in hosts + ipconfig /flushdns
String cpuv , gput , cput , cpul , gpul , gpun , cpun , ram;
long long int mil = 0 , failcount = 0;

WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);

String conv(byte* s , int l)                                          //convert payload to string
{
  String ret;
  for (int i = 0 ; i < l ; i++)
    ret += char(s[i]);
  return ret;
}

void callback(char* topic, byte* payload, unsigned int length)        //pubusb callback
{
  String T = topic;
  
  if (T == "CPU/Voltage")                                             //hardware monitor subs
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
  
  else if (T == "display/reset" && payload[0] == '1')                                     //display mode
    ESP.restart();

  else if(T == "display/config" && payload[0] == '1')
  {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.setFont(Dialog_plain_16);
    display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Config Portal");
    display.display();
    wifiManager.resetSettings();
    wifiManager.autoConnect("OLED Display");
  }
  
  else if (T == "update")                                           //update
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
    if (client.connect(clientId.c_str() , "display/online" , 2 , true, "0"))
    {
      display.drawString(display.getWidth() / 2  + 32 , 48 , "+");
      display.display();
      client.subscribe("display/config");
      client.subscribe("display/reset");
      client.subscribe("CPU/Voltage");
      client.subscribe("CPU/Temperature");
      client.subscribe("GPU/Temperature");
      client.subscribe("CPU/Load");
      client.subscribe("GPU/Load");
      client.subscribe("CPU/Name");
      client.subscribe("GPU/Name");
      client.subscribe("Mem/Load");
      client.subscribe("update");
      client.publish("display/online", "1" , true);
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
      delay(500);
    }
  }
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
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth() / 2, 16 , "Connected");
  display.drawString(display.getWidth() / 2, 32 , WiFi.localIP().toString());
  display.setFont(Dialog_plain_16);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() 
{
  ArduinoOTA.handle();
  if (millis() - mil >= 500 && client.connected())                              //hwmonitor
    hwmonitor();
  if (WiFi.status() != WL_CONNECTED || failcount >= 5)                          //restart
    ESP.restart();
  if (!client.connected() && WiFi.status() == WL_CONNECTED)                     //reconnect/connect mqtt
    reconnect();
  client.loop();
}


void hwmonitor()
{
  mil = millis();
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
  display.drawString(80, 52 , "GB");
  display.drawString(72, 12 , "°C");
  display.drawString(72, 38 , "°C");
  display.drawString(120, 12 , "%");
  display.drawString(120, 38 , "%");
  display.display();
}