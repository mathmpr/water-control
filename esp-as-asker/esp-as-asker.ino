#include <SimpleMap.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <Arduino.h>
#include "config.h"

bool hasInternet;
bool connected;
const char* checkURL = "https://www.google.com";
bool ledState = HIGH;
String responseString;
String fullUrl;

const int TX = 1;
const int GPIO0 = 0;

char ssid[20];
char password[15];

const int CONNECTING  = 500;

const int WATCHDOG_INTERVAL = 10000;

struct Timer {
  char key[20];
  unsigned long lastTick;

  int interval;
  void (*callback)();
};

const int timerSize = 10;
int timerIndex = 0;

Timer timers[timerSize];

int getTimerIndex(const char* key) {
  for(int x = 0; x < timerIndex; x++) {
    if (strcmp(timers[x].key, key) == 0) {
      return x;
    }
  }
  return -1;
}

void setTimer(const char* key, int interval, void (*callback)()) {
  int index = getTimerIndex(key);
  if (index > -1) {
    strncpy(timers[index].key, key, sizeof(timers[index].key));
    timers[index].interval = interval;
    timers[index].callback = callback;
  } else {
    strncpy(timers[timerIndex].key, key, sizeof(timers[timerIndex].key));
    timers[timerIndex].callback = callback;
    timers[timerIndex].interval = interval;
    timers[timerIndex].lastTick = 0;
    timerIndex++;
  }
}

void setTimer(const char* key, int interval) {
  int index = getTimerIndex(key);
  if (index > -1) {
    strncpy(timers[index].key, key, sizeof(timers[index].key));
    timers[index].interval = interval;
  } else {
    strncpy(timers[timerIndex].key, key, sizeof(timers[timerIndex].key));
    timers[timerIndex].interval = interval;
    timers[timerIndex].lastTick = 0;
    timerIndex++;
  }
}

void performBlink() {
  ledState = !ledState;
  digitalWrite(LED_BUILTIN, ledState);
}

SimpleMap mapReceiver;

void clear() {
  setTimer("blink", 0);
}

void blink(int interval) {
  setTimer("blink", interval);
}

void run() {
  unsigned long currentTime = millis();
  for(int x = 0; x < timerIndex; x++) {
    if (timers[x].interval == 0) {
      continue;
    }
    if(currentTime - timers[x].lastTick >= timers[x].interval) {
      timers[x].lastTick = currentTime;
      timers[x].callback();
    }
  }
}

int maxAttempts;
int maxNoInternet;
int begin;
int noInternet;
bool configured;
bool underTry;
bool doingRequest = false;
unsigned long ping = 0;
unsigned long noReset = 0;
long pingTimeout = 20000;

int offDistance = 60;
int onDistance = 200;
int minutes = 15;
bool status = true;
int interval = 5000;
unsigned long limit = 0;
unsigned long limitCount = 0;
bool reset = false;

String profile = "asker";
float distance = 0;
float water = 0; 
bool onRequest = false;

void makeRequest() {
  if (status) {
    limitCount += interval;
  }
  if (status && limitCount >= limit) {
    digitalWrite(TX, HIGH);
  }
  if (doingRequest) {
    return;
  }
  if (!connected || !hasInternet) {
    return;
  }
  if(!onRequest && profile.length() > 0) {
    requestAsker();
  }
}

void getConfig() {
  if (doingRequest) {
    return;
  }
  if (!connected || !hasInternet || profile.length() == 0) {
    return;
  }
  mapReceiver.clear();
  mapReceiver.add("wb", "gc");
  mapReceiver.add("v", profile.c_str());
  request();
  if (mapReceiver.get("wb").length() > 0) {
    minutes = mapReceiver.get("m").toInt();
    limit = 1000 * 60 * minutes;
  }
  if (!configured) {
    setTimer("get-config", (1000 * 60 * 5) + 2000);
    configured = true;
  }
}

void requestAsker() {
  ping = millis();
  mapReceiver.clear();
  mapReceiver.add("wb", "s");
  mapReceiver.add("iam", profile.c_str());
  mapReceiver.add("p", String(ping).c_str());
  if (status && limitCount >= limit) {
    mapReceiver.add("fo", "t");
    digitalWrite(TX, HIGH);
    limitCount = 0;
    status = false;
  }
  request();
  if (mapReceiver.get("wb").length() > 0) {
    if (mapReceiver.get("s") == "t") {
      status = true;
      digitalWrite(TX, LOW);
    } else {
      status = false;
      limitCount = 0;
      digitalWrite(TX, HIGH);
    }
  }
}

void request() {
  doingRequest = true;
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  fullUrl = String(endpoint) + mapReceiver.toUrlString();
  http.begin(client, fullUrl);
  int httpCode = http.GET();
  if (httpCode > 0) {
    mapReceiver.fromUrlString(http.getString().c_str());
  } else {
    mapReceiver.clear();
  }
  doingRequest = false;
}

void checkInternet() {
  if (!connected) {
    hasInternet = false;
    return;
  }
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  onRequest = true;
  http.begin(client, checkURL);
  int httpCode = http.GET();
  if (httpCode > 0) {
    hasInternet = true;
  } else {
    hasInternet = false;
    connected = false;
    WiFi.disconnect();
  }
  onRequest = false;
}

void checkConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    connected = true;
    clear();
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    connected = false;
    blink(CONNECTING);
  }
}

void connectToWifi() {
  checkConnection();
  if (connected) {
    return;
  }
  if(underTry) {
    begin += 1;
    if (begin >= size) {
      begin = 0;
    }
    underTry = false;
  }
  if(!connected && !underTry) {
    underTry = true;
    strncpy(ssid, credentials[begin].ssid, sizeof(ssid));
    strncpy(password, credentials[begin].password, sizeof(password));
    WiFi.begin(ssid, password);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(TX, OUTPUT);
  digitalWrite(TX, HIGH);

  limit = 1000 * 60 * minutes;
  limitCount = limit;
  hasInternet = false;
  connected = false;
  maxAttempts = 20;
  maxNoInternet = 5;
  begin = 0;
  noInternet = 0;
  underTry = false;
  configured = false;
  
  delay(1000);

  setTimer("blink", 0, performBlink);
  blink(CONNECTING);
  connectToWifi();
  setTimer("connect-wifi", 25000, connectToWifi);
  setTimer("check-internet", 10000, checkInternet);
  setTimer("make-request", 5000, makeRequest);
  setTimer("get-config", 8000, getConfig);

  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
}

void loop() {
  ESP.wdtFeed();
  run();
}

