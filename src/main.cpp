#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <credentials.h>
#include "time.h"
#include "sntp.h"

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

const char* time_zone = "EET-2EEST,M3.5.0/3,M10.5.0/4";  // TimeZone rule for Europe/Helsinki including daylight adjustment rules (optional)

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
  Serial.println("Got time adjustment from NTP!");
  printLocalTime();
}


TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

WiFiMulti wifiMulti;

const static int updateCycle = 5;
int lastUpdate = 0;

#define NUM_STOPTIMES 6
struct stoptime_t
{
  int stopId;
  int realtimeDeparture;
  const char* headsign;
};
stoptime_t stoptimes[NUM_STOPTIMES];

void updateDepartureTimes() {
  // check departure times frok HSL API and update list of upcoming departures
  if((wifiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;
    StaticJsonDocument<800> doc;

    //client.setCACert(root_ca);
    Serial.print("[HTTP] begin...\n");
    http.begin("https://api.digitransit.fi/routing/v1/routers/hsl/index/graphql" );

    // add necessary headers
    http.addHeader("Content-Type",   "application/json");
    //http.addHeader("Authorization",  "Bearer actual-token-goes-here");

    // send POST request
    doc["query"] = "{\n stop1: stop(id: \"HSL:2232238\") {\n name\n stoptimesWithoutPatterns(numberOfDepartures: 2) {\n realtimeDeparture\n realtime\n headsign\n }\n }\n stop2: stop(id: \"HSL:2232239\") {\n name\n stoptimesWithoutPatterns(numberOfDepartures: 2) {\n realtimeDeparture\n realtime\n headsign\n }\n }\n stop3: stop(id: \"HSL:2232211\") {\n name\n stoptimesWithoutPatterns(numberOfDepartures: 2) {\n realtimeDeparture\n realtime\n headsign\n }\n }\n}\n";
    doc["variables"] = nullptr;
    String payload; //= "{\"query\": \"{stops {name}}\" }";
    serializeJson(doc, payload);
    Serial.println(payload);

    int response = http.POST(payload);
    if (response == HTTP_CODE_OK) {
      deserializeJson(doc, http.getString());
      serializeJson(doc["data"], Serial);
      Serial.println();

      tft.setCursor(0,0);
      tft.fillScreen(TFT_BLUE);

      //Transpose incoming data into a list of departures

      stoptime_t tmpStoptimes[NUM_STOPTIMES];
      for (size_t i = 0; i < NUM_STOPTIMES; i++)
      {
        int stop = (int) i / 2 + 1;
        char stopN[6];
        snprintf(stopN, 6, "stop%d", stop);
        tmpStoptimes[i].stopId = stop;
        tmpStoptimes[i].realtimeDeparture = doc["data"][stopN]["stoptimesWithoutPatterns"][i%2]["realtimeDeparture"];
        tmpStoptimes[i].headsign = doc["data"][stopN]["stoptimesWithoutPatterns"][i%2]["headsign"];
        Serial.println(stopN);
        Serial.println(tmpStoptimes[i].realtimeDeparture);
        Serial.println(tmpStoptimes[i].headsign);
        tft.printf("%d: %s\n",tmpStoptimes[i].realtimeDeparture, tmpStoptimes[i].headsign);
      }

      

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

  sntp_set_time_sync_notification_cb( timeavailable );
  sntp_servermode_dhcp(1);
  configTzTime(time_zone, ntpServer1, ntpServer2);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLUE);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  delay(10);
  wifiMulti.addAP(SSID, WIFI_PASSWORD);
}

void loop() {
  if (millis() > (lastUpdate + updateCycle*1000)) {
    lastUpdate = millis();
    printLocalTime();
    updateDepartureTimes();
  }
}