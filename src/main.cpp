#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "time.h"
#include "sntp.h"
#include <algorithm>
#include <hsl-request.h>

#include <credentials.h>

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

const char* time_zone = "EET-2EEST,M3.5.0/3,M10.5.0/4";  // TimeZone rule for Europe/Helsinki including daylight adjustment rules

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



void setup() {
  Serial.begin(115200);

  //setup NTP time synchronisation
  sntp_set_time_sync_notification_cb( timeavailable );
  sntp_servermode_dhcp(1);
  configTzTime(time_zone, ntpServer1, ntpServer2);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(HSL_BLUE);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, HSL_BLUE, true);
  tft.setCursor(MARGINS, MARGINS);
  tft.setSwapBytes(true);
  delay(10);

  wifiMulti.addAP(SSID, WIFI_PASSWORD);
#ifdef SSID2
  wifiMulti.addAP(SSID2, WIFI_PASSWORD2);
#endif
}

void loop() {
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

        int timtableMarginY = MARGINS*2 + bicycleHeight;
        int lineHeight = 16 + PADDING;
        int headsignColumn = 80;
        for (size_t i = 0; i < 4; i++)
        {
          tm departureDay;
          localtime_r(&stoptimes[i].day, &departureDay);
          int timeUntilDeparture = stoptimes[i].realtimeDeparture - getLocalTimeOfTheDay();
          if (departureDay.tm_yday != timeinfo.tm_yday) timeUntilDeparture += 86400;
          String departureTime;
          if (timeUntilDeparture < 600) {
            departureTime = (String)(int)(timeUntilDeparture / 60) + " min";
          } else {
            int hours = (int)(stoptimes[i].realtimeDeparture / 3600);
            departureTime = leadingZero(hours % 24) + ":" + leadingZero((int)((stoptimes[i].realtimeDeparture - hours * 3600) / 60));
          }
          tft.setCursor(MARGINS, MARGINS*2 + bicycleHeight + PADDING + i * lineHeight);
          tft.print(departureTime);
          int xwidth = tft.drawString(stoptimes[i].headsign, MARGINS + headsignColumn, MARGINS*2 + bicycleHeight + PADDING + i * lineHeight);
          // use the width of the headsign text and draw a rect for the rest of the line to clear any overhanging text
          tft.fillRect(MARGINS + headsignColumn + xwidth, MARGINS*2 + bicycleHeight + PADDING + i * lineHeight, tft.width() - (MARGINS + headsignColumn + xwidth), lineHeight, HSL_BLUE);
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