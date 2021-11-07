#include "SH1106Brzo.h"                     //esp8266 oled1306  by      thingpulse
#include <WiFiManager.h>                    //wifimanager       by      tzapu
#include <ArduinoJson.h>                    //arduinojson       by      benoit
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>


#ifdef DEBUG_ESP_PORT
#define DEBUG_MSG(...) DEBUG_ESP_PORT.println( __VA_ARGS__ )
#else
#define DEBUG_MSG(...)
#endif

SH1106Brzo display(0x3c, 2 , 0);    //d1 d2
WiFiManager wifiManager;

const int HWver = 200814;                                             //Hardware version compared with a sub value
String clientId = String(ESP.getChipId());

// Fingerprint for api.cryptonator.com - expires 6 Feb 2022
const uint8_t fingerprint[20] = {0x10, 0x76, 0x19, 0x6B, 0xE9, 0xE5, 0x87, 0x5A, 0x26, 0x12, 0x15, 0xDE, 0x9F, 0x7D, 0x3B, 0x92, 0x9A, 0x7F, 0x30, 0x13};

struct crypto { 
   String coin;
   String url;
   String price;
   String change;
};

crypto cryptos[] = {
    {"BTC"  , "https://api.cryptonator.com/api/ticker/btc-usdt"  ,"",""},
    {"ETH"  , "https://api.cryptonator.com/api/ticker/eth-usdt"  ,"",""},
    {"RVN"  , "https://api.cryptonator.com/api/ticker/rvn-usdt"  ,"",""},
    {"ADA"  , "https://api.cryptonator.com/api/ticker/ada-usdt"  ,"",""},
    {"Doge" , "https://api.cryptonator.com/api/ticker/doge-usdt" ,"",""},
};
const int maxCoins = sizeof(cryptos)/sizeof(cryptos[0]);

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
  display.flipScreenVertically();
  display.setColor(WHITE);
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth() / 2, display.getHeight() / 2, "Connecting");
  display.display();
  
  WiFi.hostname("Crypto Display");
  wifiManager.setTimeout(90);
  wifiManager.autoConnect("Crypto Display");
  DEBUG_MSG("\t\t\t\t\t---Connected---");
  ArduinoOTA.setHostname("Crypto Display");
  ArduinoOTA.begin();
  MDNS.begin("Crypto Display");
  WiFi.mode(WIFI_STA);
}

int itt = 0;

unsigned long long int mil = -30000;

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if(millis() - mil > 30000)
    {
      fetchprice();
      print();
      itt++;
      mil = millis();
      //burn-in prevention
      /*if(itt%2)
        display.invertDisplay();
      else display.normalDisplay();*/
    }
  } else  ESP.restart();
  ArduinoOTA.handle();
}

void fetchprice()
{
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setFingerprint(fingerprint);
  HTTPClient https;
  for(int i = 0 ; i < maxCoins ; i++)
  {
    if (https.begin(*client, cryptos[i].url)) 
    {
      int httpCode = https.GET();
      char buf[150];
      printf(buf , "HTTPS GET... code: %d\n", httpCode);
      DEBUG_MSG(buf);
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        float price = payload.substring(payload.indexOf("price")+8,payload.indexOf("volume")-3).toFloat();
        float change = payload.substring(payload.indexOf("change")+9,payload.indexOf("timestamp")-4).toFloat();
        float changePercent = (change*100/price);
        char buffer[5];
        cryptos[i].change = dtostrf(changePercent , 1 , 1 , buffer);
        cryptos[i].price = dtostrf(price , 6 , 5 , buffer);
        cryptos[i].price = cryptos[i].price.substring(0 , 6);
        if(cryptos[i].price[5] == '.')
          cryptos[i].price[5] = '\0';
        DEBUG_MSG(payload);
        Serial.print(cryptos[i].coin + ":\t");
        Serial.print(cryptos[i].price + "$\t\t");
        Serial.println(cryptos[i].change+ '%');
      } 
      else{
        char buf[150];
        sprintf(buf , "[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        DEBUG_MSG(buf);
      }
    }
    https.end();
  }
  Serial.println("_______________________________________________________________________________________");
}

void print()
{
  display.clear();
  display.setFont(ArialMT_Plain_10);
  for(int i = 0 ; i < maxCoins ; i++)
  {
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0  , display.getHeight()*i/(maxCoins) , cryptos[i].coin);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(display.getWidth() / 2 , display.getHeight()*i/(maxCoins) + 7 , cryptos[i].price);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128 , display.getHeight()*i/(maxCoins) , cryptos[i].change + '%');
    display.display();
  }
}
