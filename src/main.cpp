#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "time.h"
#include "sntp.h"
#include <algorithm>
#include <hsl-request.h>
#include <Button2.h>
#include <credentials.h>
#include "NotoSansBold15.h"

// The font names are arrays references, thus must NOT be in quotes ""
#define AA_FONT_SMALL NotoSansBold15

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* time_zone = "EET-2EEST,M3.5.0/3,M10.5.0/4";  // TimeZone rule for Europe/Helsinki including daylight adjustment rules

#define BUTTON_1            35
#define BUTTON_2            0

Button2 btn1(BUTTON_1);
// Button2 btn2(BUTTON_2);

#define HSL_BLUE 0x0BDE
#define MARGINS 6
#define PADDING 5
#include <hsl-bicycle.h>
const static int refreshCycle = 500;
unsigned long lastRefresh = 0;
TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

WiFiMulti wifiMulti;

String leadingZero(int d) {
  if (d < 10) return "0" + (String) d;
  return (String) d;
}

int getLocalTimeOfTheDay()
{
  struct tm ti;
  if(!getLocalTime(&ti,1000)){
    Serial.println("Could not obtain time info");
    return -1;
  }
  return (ti.tm_hour * 3600 + ti.tm_min * 60 + ti.tm_sec);
}

// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
  Serial.printf("Got time adjustment from NTP!");
}

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms)
{
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

void setup() {
  Serial.begin(115200);

  //setup NTP time synchronisation
  sntp_set_time_sync_notification_cb( timeavailable );
  sntp_servermode_dhcp(1);
  configTzTime(time_zone, ntpServer1, ntpServer2);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(HSL_BLUE);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, HSL_BLUE, true);
  tft.setCursor(MARGINS, MARGINS);
  tft.setSwapBytes(true);
  tft.loadFont(AA_FONT_SMALL); // Muste load the font first
  delay(10);

  tft.print("Starting up...");

  btn1.setLongClickHandler([](Button2 & b) {
      // btnCick = false;
      int r = digitalRead(TFT_BL);
      tft.fillScreen(HSL_BLUE);
      tft.setTextColor(TFT_WHITE, HSL_BLUE);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Press again to wake up",  tft.width() / 2, tft.height() / 2 );
      espDelay(6000);
      digitalWrite(TFT_BL, !r);

      tft.writecommand(TFT_DISPOFF);
      tft.writecommand(TFT_SLPIN);
      //After using light sleep, you need to disable timer wake, because here use external IO port to wake up
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
      // esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);
      delay(200);
      esp_deep_sleep_start();
  });

  wifiMulti.addAP(SSID, WIFI_PASSWORD);
#ifdef SSID2
  wifiMulti.addAP(SSID2, WIFI_PASSWORD2);
#endif
}

void loop() {
  btn1.loop();
  updateDepartureTimes();
  if (millis() > (lastRefresh + refreshCycle)) {
    lastRefresh = millis();
    if (dataAvailable) {
      struct tm timeinfo;
      if(!getLocalTime(&timeinfo)){
        Serial.println("No time available (yet)");
      }
      else {
        tft.setCursor(MARGINS,MARGINS + (int)(bicycleHeight/2) - 8 );
        tft.println(&timeinfo, "%H:%M:%S");

        int timetableMarginY = MARGINS*2 + bicycleHeight + PADDING;
        int lineHeight = 16 + PADDING;
        int routeX = MARGINS + 55;
        int headsignX = routeX + 45;
        for (size_t i = 0; i < 4; i++)
        {
          tm departureDay;
          localtime_r(&stoptimes[i].day, &departureDay);
          int timeUntilDeparture = stoptimes[i].realtimeDeparture - getLocalTimeOfTheDay();
          if (departureDay.tm_yday != timeinfo.tm_yday) timeUntilDeparture += 86400;
          String departureTime;
          if (timeUntilDeparture % 86400 < 600) {
            departureTime = (String)(int)(timeUntilDeparture / 60) + " min";
          } else {
            int hours = (int)(stoptimes[i].realtimeDeparture / 3600);
            departureTime = leadingZero(hours % 24) + ":" + leadingZero((int)((stoptimes[i].realtimeDeparture - hours * 3600) / 60));
          }
          int xwidth = tft.drawString(departureTime, MARGINS, timetableMarginY + i * lineHeight);
          tft.fillRect(MARGINS + xwidth, timetableMarginY + i * lineHeight, routeX - (MARGINS + xwidth), lineHeight, HSL_BLUE); // use the width of the headsign text and draw a rect for the rest of the line to clear any overhanging text

          xwidth = tft.drawString(stoptimes[i].route, routeX, timetableMarginY + i * lineHeight);
          tft.fillRect(routeX + xwidth, timetableMarginY + i * lineHeight, headsignX - routeX - xwidth, lineHeight, HSL_BLUE);
          
          xwidth = tft.drawString(stoptimes[i].headsign, headsignX, timetableMarginY + i * lineHeight);
          tft.fillRect(headsignX + xwidth, timetableMarginY + i * lineHeight, tft.width() - headsignX - xwidth, lineHeight, HSL_BLUE);
        }

        if (bikesAvailable > 0) {
          tft.pushImage(tft.width()-bicycleWidth-MARGINS,MARGINS, bicycleWidth, bicycleHeight, bicycle);
        } else {
          tft.drawRect(tft.width()-bicycleWidth-MARGINS,MARGINS, bicycleWidth, bicycleHeight, HSL_BLUE);
        }
      }
    }
  }
}