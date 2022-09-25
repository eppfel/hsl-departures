#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Ticker.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <credentials.h>

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

WiFiMulti wifiMulti;

const static int updateCycle = 5;
int lastUpdate = 0;
Ticker tickerDepartureT;

void updateDepartureTimes() {
  // check departure times frok HSL API and update list of upcoming departures
  if((wifiMulti.run() == WL_CONNECTED)) {

    HTTPClient http;
    StaticJsonDocument<400> doc;

    //client.setCACert(root_ca);
    Serial.print("[HTTP] begin...\n");
    http.begin("https://api.digitransit.fi/routing/v1/routers/hsl/index/graphql" );

    // add necessary headers
    http.addHeader("Content-Type",   "application/json");
    //http.addHeader("Authorization",  "Bearer actual-token-goes-here");

    // send POST request
    doc["query"] = "{\n stop: stop(id: \"HSL:2232238\") {\n name\n stoptimesWithoutPatterns(numberOfDepartures: 2) {\n realtimeArrival\n headsign\n }\n }\n}\n";
    doc["variables"] = nullptr;
    String payload; //= "{\"query\": \"{stops {name}}\" }";
    serializeJson(doc, payload);
    Serial.println(payload);

    int response = http.POST(payload);
    if (response == HTTP_CODE_OK) {
      deserializeJson(doc, http.getString());
      serializeJson(doc["data"], Serial);
      Serial.println();
      tft.println("OK!");
    } else {
      //Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(response).c_str());
      Serial.print("[HTTO] Error: ");
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
  Serial.println();

  // for(uint8_t t = 4; t > 0; t--) {
  //     Serial.printf("[SETUP] WAIT %d...\n", t);
  //     Serial.flush();
  //     delay(1000);
  // }

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
  
  //tickerDepartureT.attach(5, updateDepartureTimes);
}

void loop() {
  if (millis() > (lastUpdate + updateCycle*1000)) {
    lastUpdate = millis();
    updateDepartureTimes();
  }
}