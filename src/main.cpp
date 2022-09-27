#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include "sntp.h"
#include <algorithm>

#include <credentials.h>

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

const char* time_zone = "EET-2EEST,M3.5.0/3,M10.5.0/4";  // TimeZone rule for Europe/Helsinki including daylight adjustment rules (optional)

#define HSL_BLUE 0x0BDE
#define MARGINS 6
#define PADDING 5
#include <hsl-bicycle.h>
int lastRefresh;
const static int refreshCycle = 500;
TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

WiFiMulti wifiMulti;

const static int updateCycle = 15;
int lastUpdate = 0;
#define NUM_STOPTIMES 8
struct stoptime_t
{
  int stopId;
  time_t day;
  int realtimeDeparture;
  const char* headsign;
  const char* route;
};
stoptime_t stoptimes[NUM_STOPTIMES];
int bikesAvailable;

bool cmpfunc(stoptime_t a, stoptime_t b) { 
  if (a.day == b.day) {
    return a.realtimeDeparture < b.realtimeDeparture;
  }
  return a.day < b.day;
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

void updateDepartureTimes() {
  // check departure times frok HSL API and update list of upcoming departures
  if((wifiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;
    StaticJsonDocument<2048> doc;

    //client.setCACert(root_ca);
    Serial.print("[HTTP] begin...\n");
    http.begin("https://api.digitransit.fi/routing/v1/routers/hsl/index/graphql" );

    // add necessary headers
    http.addHeader("Content-Type",   "application/json");

    // send POST request
    doc["query"] = "{ stop1: stop(id: \"HSL:2232238\") { name stoptimesWithoutPatterns(numberOfDepartures: 2) { realtimeDeparture serviceDay trip { routeShortName tripHeadsign } } } stop2: stop(id: \"HSL:2232239\") { name stoptimesWithoutPatterns(numberOfDepartures: 2) { realtimeDeparture serviceDay trip { routeShortName tripHeadsign } } } stop3: stop(id: \"HSL:2232228\") { name name stoptimesWithoutPatterns(numberOfDepartures: 2) { realtimeDeparture serviceDay trip { routeShortName tripHeadsign } } } stop4: stop(id: \"HSL:2232232\") { name stoptimesWithoutPatterns(numberOfDepartures: 2) { realtimeDeparture serviceDay trip { routeShortName tripHeadsign } } } bikes: bikeRentalStation(id:\"587\") {bikesAvailable} }";
    doc["variables"] = nullptr;
    String payload; //= "{\"query\": \"{stops {name}}\" }";
    serializeJson(doc, payload);
    Serial.println(payload);

    int response = http.POST(payload);
    if (response == HTTP_CODE_OK) {
      deserializeJson(doc, http.getString());
      serializeJson(doc["data"], Serial);
      Serial.println();
      //JsonObject ddat = doc["data"];

      //Transpose incoming data into a list of departures
      for (size_t i = 0; i < NUM_STOPTIMES; i++)
      {
        int stop = (int) i / 2 + 1;
        char stopN[6];
        snprintf(stopN, 6, "stop%d", stop);
        stoptimes[i].stopId = stop;
        stoptimes[i].realtimeDeparture = doc["data"][stopN]["stoptimesWithoutPatterns"][i%2]["realtimeDeparture"];
        stoptimes[i].headsign = doc["data"][stopN]["stoptimesWithoutPatterns"][i%2]["trip"]["tripHeadsign"];
        stoptimes[i].route = doc["data"][stopN]["stoptimesWithoutPatterns"][i%2]["trip"]["routeShortName"];
        stoptimes[i].day = doc["data"][stopN]["stoptimesWithoutPatterns"][i%2]["serviceDay"];
        Serial.println(stopN);
        Serial.println(stoptimes[i].realtimeDeparture);
        Serial.println(stoptimes[i].headsign);
        Serial.println(stoptimes[i].route);
        //Serial.printf("%s: %s %s at %d\n",stopN, stoptimes[i].route, stoptimes[i].headsign, stoptimes[i].realtimeDeparture);
      }
      std::sort(std::begin(stoptimes), std::end(stoptimes), cmpfunc);

      bikesAvailable = doc["data"]["bikes"]["bikesAvailable"];

    } else {
      Serial.print("[HTTP] Error: ");
      Serial.println(response);
      tft.println("Error!");
      payload = http.getString();
      Serial.println(payload);
    }

    http.end();
  } else {
    Serial.println("Not connected!");
    tft.println("Not connected!");
  }
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
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setSwapBytes(true);
  delay(10);

  wifiMulti.addAP(SSID, WIFI_PASSWORD);
#ifdef SSID2
  wifiMulti.addAP(SSID2, WIFI_PASSWORD2);
#endif
}

void loop() {
  if (millis() > (lastUpdate + updateCycle*1000)) {
    lastUpdate = millis();
    updateDepartureTimes();
  }
  if (millis() > (lastRefresh + refreshCycle)) {
    lastRefresh = millis();
    if (stoptimes[0].realtimeDeparture > 0) {
      struct tm timeinfo;
      if(!getLocalTime(&timeinfo)){
        Serial.println("No time available (yet)");
      }
      else {
        tft.setCursor(MARGINS,MARGINS + (int)(bicycleHeight/2) - 8 );
        //tft.fillScreen(HSL_BLUE);
        tft.println(&timeinfo, "%H:%M:%S");
        int timtableMarginY = MARGINS*2 + bicycleHeight;
        int lineHeight = 16 + PADDING;
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
            departureTime = (String) hours + ":" + (String)(int)((stoptimes[i].realtimeDeparture - hours * 3600) / 60);
          }
          tft.setCursor(MARGINS, MARGINS*2 + bicycleHeight + PADDING + i * lineHeight);
          tft.print(departureTime);
          tft.setCursor(MARGINS + 80, MARGINS*2 + bicycleHeight + PADDING + i * lineHeight);
          tft.print(stoptimes[i].headsign);
          //tft.println(departureTime + " " + stoptimes[i].route + " " + stoptimes[i].headsign);
          //tft.printf("%s %s\n", departureTime, stoptimes[i].route);
          //tft.printf("%d: %s %s\n", stoptimes[i].realtimeDeparture - getLocalTimeOfTheDay(), stoptimes[i].route, stoptimes[i].headsign); //unicode characters create problems
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