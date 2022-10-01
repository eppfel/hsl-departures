#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define NUM_STOPTIMES 8 // this needs to corellate with the API request: currently 4 stations x 2 departures

struct stoptime_t
{
  int stopId;
  time_t day;
  int realtimeDeparture;
  const char* headsign;
  const char* route;
};

extern stoptime_t stoptimes[NUM_STOPTIMES];

extern String errorMessage;

extern int bikesAvailable;

extern bool dataAvailable;

extern WiFiMulti wifiMulti;

int updateDepartureTimes();