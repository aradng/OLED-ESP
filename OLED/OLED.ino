/*
MQTT topidcs 
  *display
    *text     : OUTPUT for display
    *bitmap   : display bitmaps
    *state    : 0(LCD_OFF) / 1(LCD_ON) / 3(toggle ping on/off) 
  *update   : channel sync for hardware version
*/
/*
For Bitmap converts :
	image to hex converter : http://javl.github.io/image2cpp/
	hex to base64 converter : https://base64.guru/converter/encode/hex
*/
#include <Wire.h>
#include <base64.hpp>
#include <DHT12.h>
#include <Pinger.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <Adafruit_SSD1306.h>                 //https://github.com/WaiakeaRobotics/Adafruit_SSD1306
#include <ESP8266httpUpdate.h>
#include <fonts/Org_01.h>

#define SSD1306_LCDWIDTH 64
#define SSD1306_LCDHEIGHT 32
Adafruit_SSD1306 display(-1);

const int HWver = 191220;                      //Hardware version compared with a sub value
String clientId = String(ESP.getChipId());
int failcount;
const char *mqtt_server = "192.168.1.3";
const char *local_server = "arad-pc.local";      //MQTT server address(add A/CNAME in hosts + ipconfig /flushdns
String cpuv , gput , cput , cpul , gpul , gpun , cpun , ram;
bool pingOn = 0,done = 1,weatherOn = 0,hwmon = 0,rst = 0,drawOn = 0;

DHT12 dht12;
WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);
Pinger pinger;

String conv(byte* s , int l)
{
  String ret;
  for(int i = 0 ; i < l ; i++)
    ret += char(s[i]);
  return ret;
}
void callback(char* topic, byte* payload, unsigned int length)
{
  String T = topic;
  if(T == "CPU/Voltage")
    cpuv = conv(payload,length);
  else if(T == "CPU/Temperature")
    cput = conv(payload,length);
  else if(T == "GPU/Temperature")
    gput = conv(payload,length);
  else if(T == "GPU/Load")
    gpul = conv(payload,length);
  else if(T == "CPU/Load")
    cpul = conv(payload,length);
  else if(T == "Memory/Load")
    ram = conv(payload,length);
  else if(T == "CPU/Name")
    cpun = conv(payload,length);
  else if(T == "GPU/Name")
    gpun = conv(payload,length);
  
  else if(T == "display/text")
  {
    display.setTextSize((char)payload[0] - '0');
    display.setCursor(0,0);
    display.clearDisplay();
    for(int i = 1;i < length; i++)
      display.print((char)payload[i]);
    display.display();
  }
  else if(T == "display/state")
  {
    if((char)payload[0] == '0')
      display.ssd1306_command(SSD1306_DISPLAYOFF);
    else if ((char)payload[0] == '1')
      display.ssd1306_command(SSD1306_DISPLAYON);
    else if ((char)payload[0] == '2')
    {
      pingOn = !pingOn;
      client.publish("ack", "ping");
    }
    else if((char)payload[0] == '3')
    {
      hwmon = !hwmon;
      client.publish("ack", "hwmode");
    }
    else if((char)payload[0] == '4')
    {
      drawOn = !drawOn;
      client.publish("ack", "displaymode");
    }
    else if((char)payload[0] == '5')
      ESP.restart();
  }
  else if(T == "display/bitmap")
  {
    unsigned char out[256];
    decode_base64(payload,out);
    display.clearDisplay();
    display.drawBitmap(0, 0, out, 64, 32, WHITE);
    display.display();
  }
  else if(T == "update")
  {
    int hw = 0,pow = 100000;
    for(int i = 0 ; i < 6 ; i++)
    {
      hw += (payload[i] - '0')* pow;
      pow /= 10;
    }
    if(HWver < hw)
    {
      WiFiClient netClient;
      ESPhttpUpdate.update(netClient, "http://oled.aradng.ml/OLED/OLED.ino.generic.bin");
    }
  }
}

void update_started() {
    display.setTextSize(1);
    display.clearDisplay();
    display.setCursor(2,4);
    display.print("OTA Update");
    display.display();
}

void update_progress(int cur, int total) {
    drawProgressbar(0,20,64,10, (cur*100)/total);
    display.display();
}

void update_finished() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(11,12);
    display.print("RESTART");
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
  Wire.begin(2,0);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(2,10);
  display.print("Connecting");
  display.display();
  wifiManager.setTimeout(180);
  wifiManager.autoConnect("SmartConfig");
  ArduinoOTA.begin();
  display.clearDisplay();
  display.setCursor(5,0);
  display.println("Connected");
  display.setFont(&Org_01);
  display.setCursor((64 - (WiFi.localIP().toString()).length()*4) / 2,16);
  display.print(WiFi.localIP().toString());
  display.setFont();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinger.OnReceive([](const PingerResponse& response)
  {
    int length = 0;
    int a = response.ResponseTime;
    while(a > 0)
    {
      a /= 10;
      length++;
    }
    display.setTextSize(2);
    display.clearDisplay();
    display.setCursor((32 - (length%5)*6)*(length%5 != 0),16 - 8*((length+4)/5));
    if (response.ReceivedResponse)
      display.print(response.ResponseTime);
    display.display();
    return true;
  });
  pinger.OnEnd([](const PingerResponse& response)
  {
    done = true;
    return true;
  });
}

void reconnect() 
{
  String msg;
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10,23);
  display.print("MQTT");
  display.setCursor(40,23);
  display.print('-');
  display.display();
  while (!client.connected()) 
  {
    msg = "ESP-" + clientId + " joined.";
    if (client.connect(msg.c_str()))
    {
      display.setCursor(40,23);
      display.print('+');
      display.display();
      client.subscribe("display/text");
      client.subscribe("display/state");
      client.subscribe("display/bitmap");
      client.subscribe("CPU/Voltage");
      client.subscribe("CPU/Temperature");
      client.subscribe("GPU/Temperature");
      client.subscribe("update");
      client.publish("ack", msg.c_str());
    }
    else 
    { 
      failcount++;
      if(failcount % 2)
        client.setServer(local_server, 1883);
      else 
        client.setServer(mqtt_server,1883);
      display.setTextColor(BLACK);
      display.setCursor(40,23);
      display.print('+');
      display.setCursor(40,23);
      display.setTextColor(WHITE);
      display.print('-');
      display.display();
    }
  }
}
long long int mil = 0;
int i = 0;
void loop() {
  ArduinoOTA.handle();
  if(WiFi.status() != WL_CONNECTED)
    ESP.restart();
  if(done & pingOn)
  {
    done = false;
    pinger.Ping("lux.valve.net",1);
  }
  if(drawOn)
  {
    i++;
    for(int j = 0 ; j < 64; j++)
      for(int k = 0 ; k < 32 ; k++)
        display.drawPixel(j,k,((j+i)/16+k/16)%2);
    display.display();
    //delay(50);      too slow for long gets burnout and massive current makes lcd logic unstable
    if(i >= 16 * 2)
      i = 0;
  }
  if(hwmon && (millis() - mil) >= 500)
  {
    mil = millis();
    hwmonitor();
  }
  if(!client.connected() && WiFi.status() == WL_CONNECTED)
    reconnect();
  client.loop();
}

void drawProgressbar(int x,int y, int width,int height, int progress)
{
   progress = progress > 100 ? 100 : progress;
   progress = progress < 0 ? 0 :progress;
   float bar = ((float)(width-1) / 100) * progress;
   display.drawRect(x, y, width, height, WHITE);
   display.fillRect(x+2, y+2, bar , height-4, WHITE);
}
