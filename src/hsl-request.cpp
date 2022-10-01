#include <hsl-request.h>

RTC_DATA_ATTR stoptime_t stoptimes[NUM_STOPTIMES];
String errorMessage = "";
RTC_DATA_ATTR int bikesAvailable = -1;
RTC_DATA_ATTR bool dataAvailable = false;

// send POST request to HSL API in graphql format. See https://digitransit.fi/en/developers/apis/1-routing-api/
const static char* apiURL = "https://api.digitransit.fi/routing/v1/routers/hsl/index/graphql";
const static char* queryString = "{ stop1: stop(id: \"HSL:2232238\") { name stoptimesWithoutPatterns(numberOfDepartures: 2) { realtimeDeparture serviceDay trip { routeShortName tripHeadsign } } } stop2: stop(id: \"HSL:2232239\") { name stoptimesWithoutPatterns(numberOfDepartures: 2) { realtimeDeparture serviceDay trip { routeShortName tripHeadsign } } } stop3: stop(id: \"HSL:2232228\") { name name stoptimesWithoutPatterns(numberOfDepartures: 2) { realtimeDeparture serviceDay trip { routeShortName tripHeadsign } } } stop4: stop(id: \"HSL:2232232\") { name stoptimesWithoutPatterns(numberOfDepartures: 2) { realtimeDeparture serviceDay trip { routeShortName tripHeadsign } } } bikes: bikeRentalStation(id:\"587\") {bikesAvailable} }";

const static int updateCycle = 30;
unsigned long lastUpdate = 0;

bool cmpfunc(stoptime_t a, stoptime_t b) { 
  if (a.day == b.day) {
    return a.realtimeDeparture < b.realtimeDeparture;
  }
  return a.day < b.day;
}

// check departure times from routing API and update list of upcoming departures
int updateDepartureTimes() {
  if (millis() < (lastUpdate + updateCycle*1000)) {
    return 0;
  }
  if((wifiMulti.run() == WL_CONNECTED)) {
    lastUpdate = millis();
    HTTPClient http;
    StaticJsonDocument<2048> doc;

    Serial.print("[HTTP] begin...\n");
    http.begin(apiURL);
    http.addHeader("Content-Type", "application/json");
    doc["query"] = queryString;
    doc["variables"] = nullptr;
    String payload;
    serializeJson(doc, payload);
    // Serial.println(payload);

    int response = http.POST(payload);
    if (response == HTTP_CODE_OK) {
      dataAvailable = true;
      Serial.print("[HTTP] Success!\n");
      deserializeJson(doc, http.getString());
      // serializeJson(doc["data"], Serial);
      // Serial.println();

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
        // Serial.println(stopN);
        // Serial.println(stoptimes[i].realtimeDeparture);
        // Serial.println(stoptimes[i].headsign);
        // Serial.println(stoptimes[i].route);
      }
      std::sort(std::begin(stoptimes), std::end(stoptimes), cmpfunc);

      bikesAvailable = doc["data"]["bikes"]["bikesAvailable"];
      http.end();
      return 1;
    } else {
      Serial.print("[HTTP] Error: ");
      Serial.println(response);
      payload = http.getString();
      Serial.println(payload);
      errorMessage = payload;
        http.end();
      return -1;
    }
  } else {
    Serial.println("Not connected!");
    errorMessage = "Not connected!";
    return -1;
  }

}