#include <Arduino.h>
#include <WiFi.h>
#include "WiFiManager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "TFT_eSPI.h"
#include "NotoSansBold15.h"
#include "NotoSansBold36.h"
#include "back.h"

#define AA_FONT_SMALL NotoSansBold15
#define AA_FONT_LARGE NotoSansBold36

// Replace with your network credentials
// const char *ssid = "SSID";
// const char *password = "password";

// Replace with your time zone
const char *timezone = "Europe/Budapest";

// Replace with your NTP server pool
const char *ntpServer = "de.pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
char timeHour[3];
char timeMin[3];
char timeSec[3];
char day[3];
char month[6];
char year[5];

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft); // background
TFT_eSprite sprite_second = TFT_eSprite(&tft);
TFT_eSprite sprite_fps = TFT_eSprite(&tft);
TFT_eSprite sprite_price = TFT_eSprite(&tft);
HTTPClient http;

void timeUpdate(void *arg);
void priceUpdate(void *arg);

// Time variables
unsigned long lastTime = 0;
int lastSecond = -1;
int lastMinute = -1;
String IP;

void setup()
{
  tft.init();
  tft.setRotation(3);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(7);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("ETH TICKER", 320 / 2, 170 / 2, 4);
  tft.setTextDatum(1);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString("by Rew", 320 / 2, 170 / 2, 2);
  // ledcSetup(0,5000,8);
  // ledcAttachPin(TFT_BL, 0);
  // ledcWrite(0, 50);
  sprite.createSprite(320, 170);
  sprite.setSwapBytes(true);
  sprite.setTextColor(TFT_WHITE, 0xEAA9);
  sprite.setTextDatum(MC_DATUM);
  // sprite.pushImage(0, 0, 320, 170, back);

  sprite_second.createSprite(80, 40);
  sprite_second.fillSprite(TFT_GREEN);
  sprite_second.setTextColor(TFT_WHITE, TFT_GREEN);
  sprite_second.setFreeFont(&Orbitron_Light_32);

  sprite_price.createSprite(220, 150);
  sprite_price.fillSprite(TFT_GREEN);
  sprite_price.setTextDatum(ML_DATUM);
  sprite_price.setTextColor(TFT_WHITE, TFT_GREEN);

  sprite_fps.createSprite(80, 64);
  sprite_fps.fillSprite(TFT_GREEN);
  sprite_fps.setTextDatum(MC_DATUM);
  sprite_fps.setTextColor(TFT_WHITE, TFT_GREEN);
  sprite_fps.setFreeFont(&Orbitron_Light_32);

  // WiFi part
  Serial.begin(9600);
  // WiFi.begin(ssid, password);
  WiFiManager wm;

  // wm.resetSettings();

  WiFiManagerParameter custom_text_box("my_text", "Enter your string here", "default string", 50);
  wm.addParameter(&custom_text_box);

  if (!wm.autoConnect("PriceTicker", "priceticker"))
  {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
    delay(1000);
  }
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
  Serial.print("Custom text box entry: ");
  Serial.println(custom_text_box.getValue());

  IP = WiFi.localIP().toString();
  delay(1000);

  // configTime(TIMEZONE * 3600, 0, ntpServer);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  while (!time(nullptr))
  {
    delay(1000);
    Serial.println("Waiting for time synchronization...");
  }
  Serial.println("Time synchronized!");
  delay(1000);

  // timer
  esp_timer_handle_t timer1;
  esp_timer_handle_t timer2;

  const esp_timer_create_args_t timer1_args = {
      .callback = &timeUpdate,
      .name = "clock_timer"};
  esp_timer_create(&timer1_args, &timer1);
  esp_timer_start_periodic(timer1, 1000000);

  const esp_timer_create_args_t timer2_args = {
      .callback = &priceUpdate,
      .name = "price_timer"};
  esp_timer_create(&timer2_args, &timer2);
  esp_timer_start_periodic(timer2, 60000000);
  priceUpdate(nullptr);
}

long startTime = 0;
long endTime = 0;
double fps = 0;
float price = 2234.5;
float priceChange = 0.0;
float priceChangePercent = 0.0;

void loop()
{
  startTime = millis();

  sprite_second.fillSprite(TFT_GREEN);
  sprite_price.fillSprite(TFT_GREEN);

  sprite_fps.fillSprite(TFT_GREEN);
  sprite_fps.setFreeFont(&Orbitron_Light_24);

  sprite.pushImage(0, 0, 320, 170, back);
  sprite.setTextColor(0x604D, TFT_WHITE);
  sprite.fillRoundRect(230, 8, 80, 26, 3, TFT_WHITE);
  sprite.fillRoundRect(230, 78, 80, 16, 3, TFT_WHITE);
  sprite.drawString(String(month) + "/" + String(day), 230 + 40, 8 + 70 + 8, 2);
  sprite.drawString(String(timeHour) + ":" + String(timeMin), 230 + 40, 8 + 13, 4);

  // sprite_price.drawRoundRect(0, 0, 220, 139, 3, TFT_WHITE);
  sprite_price.setTextColor(TFT_WHITE, TFT_GREEN);
  sprite_price.setFreeFont(&FreeSansBold12pt7b);
  sprite_price.drawString("ETHUSDT", 10, 12);
  sprite_price.setFreeFont(&FreeSansBold18pt7b);
  sprite_price.drawString("$", 4, 75);
  sprite_price.drawFloat(price, 2, 38, 75);
  sprite_price.setFreeFont(&FreeSansBold9pt7b);
  // if(priceChange < 0) {
  //     sprite_price.setTextColor(0xF800, TFT_GREEN);
  //   } else {
  //     sprite_price.setTextColor(TFT_DARKGREEN, TFT_GREEN);
  //   }
  sprite_price.drawFloat(priceChange, 2, 20, 120);
  // if(priceChangePercent < 0) {
  //     sprite_price.setTextColor(TFT_RED, TFT_GREEN);
  //   } else {
  //     sprite_price.setTextColor(TFT_DARKGREEN, TFT_GREEN);
  //   }
  sprite_price.drawFloat(priceChangePercent, 2, 100, 120);

  sprite_second.drawString(String(timeSec), 4, 6);

  sprite_fps.drawRoundRect(0, 0, 80, 34, 3, TFT_WHITE);
  sprite_fps.drawString("FPS", 62, 14, 2);
  sprite_fps.drawString(String((int)fps), 26, 14);
  sprite_fps.drawString("CONNECTED", 40, 44, 2);
  sprite_fps.setTextFont(0);
  sprite_fps.drawString(IP, 40, 60);

  sprite_second.pushToSprite(&sprite, 236, 30, TFT_GREEN);
  sprite_price.pushToSprite(&sprite, 0, 30, TFT_GREEN);
  sprite_fps.pushToSprite(&sprite, 230, 78 + 16 + 6, TFT_GREEN);
  sprite.pushSprite(0, 0);

  endTime = millis();
  fps = 1000 / (endTime - startTime);

  // // Get current time
  // unsigned long currentTime = millis();
  // int currentSecond = currentTime * 1000;
  // int currentMinute = currentTime * 60000;

  // // Update time every second
  // if (currentSecond != lastSecond)
  // {
  //   lastSecond = currentSecond;
  //   // updateClock();
  // }

  // // Update Ethereum price every minute
  // if (currentMinute != lastMinute)
  // {
  //   lastMinute = currentMinute;
  //   updateSprite();
  // }
}

void timeUpdate(void *arg)
{
  // time_t now;
  // time(&now);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    return;
  }

  // char time_str[64];
  // strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
  strftime(timeHour, 3, "%H", &timeinfo);
  strftime(timeMin, 3, "%M", &timeinfo);
  strftime(timeSec, 3, "%S", &timeinfo);

  strftime(day, 3, "%d", &timeinfo);
  strftime(month, 6, "%B", &timeinfo);
  // tft.fillScreen(TFT_BLACK);
  //  tft.setCursor(1, 1);
  // tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //  tft.setTextSize(3);
  // tft.drawString(time_str,1,1);
  // tft.drawLine(0,15,60,15,TFT_SKYBLUE);
  // tft.drawLine(60,0,60,15,TFT_SKYBLUE);
}

void priceUpdate(void *arg)
{
  http.begin("https://api.binance.com/api/v3/ticker?symbol=ETHUSDT&windowSize=1h");
  int httpCode = http.GET();
  Serial.println(httpCode);
  if (httpCode > 0)
  {
    String payload = http.getString();
    Serial.println(payload);
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    String ethprice = doc["weightedAvgPrice"];
    Serial.println(ethprice);
    float ethpriceChange = doc["priceChange"].as<float>();
    float ethpriceChangePercent = doc["priceChangePercent"].as<float>();
    price = ethprice.toFloat();
    Serial.println(String(price));
    priceChange = ethpriceChange;
    priceChangePercent = ethpriceChangePercent;

    // int startIndex = response.indexOf("price\":") + 8;
    // int endIndex = response.indexOf("}", startIndex);
    // price = response.substring(startIndex, endIndex - 7).toFloat();
  }
  else
  {
    price = 0.0;
  }
}