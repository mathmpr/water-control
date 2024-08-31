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
const int RX = 3;
const int GPIO0 = 0;
SoftwareSerial ss(RX, TX);

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

String receive;
String sender;
String event;
SimpleMap serialCommunication;
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

void send() {
  sender = serialCommunication.toUrlString();
  ss.println("<" + sender + ">");
  serialCommunication.clear();
}

bool received() {
  event = String();
  if (ss.available()) {
    receive = ss.readStringUntil('\n');
    int start = receive.indexOf('<');
    int end = receive.indexOf('>');
    if (start > -1 && end > -1) {
      receive = receive.substring(start + 1, end);
      serialCommunication.fromUrlString(receive.c_str());
      event = serialCommunication.get("ev");
    }
  }
  return true;
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

String profile = "";
float distance = 0;
float water = 0; 
bool onRequest = false;

void performReset() {
  reset = true;
  digitalWrite(GPIO0, LOW);
  delay(150);
  digitalWrite(GPIO0, HIGH);
}

void makeRequest() {
  if (status && profile == "asker") {
    limitCount += interval;
  }
  if (status && limitCount >= limit && profile == "asker") {
    serialCommunication.clear();
    serialCommunication.add("ev", "of");
    serialCommunication.add("v", "f");
    send();
  }
  if (doingRequest) {
    return;
  }
  if (!connected || !hasInternet) {
    return;
  }
  if(!onRequest && profile.length() > 0) {
    if (profile == "sender") {
      requestSender();
    }
    if (profile == "asker") {
      requestAsker();
    }
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
    if (profile == "sender") {
      onDistance = mapReceiver.get("td").toInt();
      offDistance = mapReceiver.get("fd").toInt();
    } else {
      minutes = mapReceiver.get("m").toInt();
      limit = 1000 * 60 * minutes;
    }
  }
  if (!configured) {
    setTimer("get-config", (1000 * 60 * 5) + 2000);
    configured = true;
  }
}

void requestSender() {
  mapReceiver.clear();
  mapReceiver.add("wb", "tn");
  mapReceiver.add("iam", profile.c_str());
  mapReceiver.add("p", String(ping).c_str());
  mapReceiver.add("d", String(distance).c_str());
  if (reset) {
    reset = false;
    mapReceiver.add("r", "t");
  }
  if (distance > onDistance) {
    mapReceiver.add("v", "t");
  }
  if (distance < offDistance) {
    mapReceiver.add("v", "f");
  }
  if (water > 0) {
    mapReceiver.add("v", "f");
    mapReceiver.add("w", "t");
  }
  distance = 0;
  water = 0;
  request();  
}

void requestAsker() {
  mapReceiver.clear();
  mapReceiver.add("wb", "s");
  mapReceiver.add("iam", profile.c_str());
  mapReceiver.add("p", String(ping).c_str());
  if (reset) {
    reset = false;
    mapReceiver.add("r", "t");
  }
  if (status && limitCount >= limit) {
    mapReceiver.add("fo", "t");
    serialCommunication.clear();
    serialCommunication.add("ev", "of");
    serialCommunication.add("v", "f");
    send();
    limitCount = 0;
    status = false;
  }
  request();
  if (mapReceiver.get("wb").length() > 0) {
    serialCommunication.clear();
    serialCommunication.add("ev", "of");
    if (mapReceiver.get("s") == "t") {
      status = true;
    } else {
      status = false;
      limitCount = 0;
    }
    serialCommunication.add("v", mapReceiver.get("s").c_str());
    send();
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
  http.begin(client, checkURL);
  int httpCode = http.GET();
  if (httpCode > 0) {
    hasInternet = true;
  } else {
    hasInternet = false;
    noInternet++;
  }
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
  if (connected && noInternet < maxNoInternet) {
    return;
  }
  if(underTry || noInternet > maxNoInternet) {
    begin += 1;
    if (begin >= size) {
      begin = 0;
    }
    underTry = false;
  }
  if (noInternet > maxNoInternet) {
    connected = false;
    noInternet = 0;
  }
  if(!connected && !underTry) {
    underTry = true;
    strncpy(ssid, credentials[begin].ssid, sizeof(ssid));
    strncpy(password, credentials[begin].password, sizeof(password));
    WiFi.begin(ssid, password);
  }
}

void resetArduino() {
  long currentTime = millis();
  if ((currentTime - noReset) > pingTimeout) {
    noReset = currentTime;
    performReset();
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(GPIO0, OUTPUT);
  digitalWrite(GPIO0, HIGH);

  Serial.begin(9600);
  ss.begin(9600);

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
  setTimer("connect-wifi", 15000, connectToWifi);
  setTimer("check-internet", 7500, checkInternet);
  setTimer("make-request", 5000, makeRequest);
  setTimer("get-config", 8000, getConfig);
  setTimer("reset-arduino", pingTimeout, resetArduino);

  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
}

void loop() {
  ESP.wdtFeed();

  if (Serial.available()) {
    ss.println(Serial.readStringUntil('\n'));
  }
  run();

  if(received()) {
    if (event.length() > 0) {
      if (event == "pr") {
        profile = serialCommunication.get("v");
        long currentTime = millis();
        ping = currentTime;
        noReset = currentTime;
      }
      if (event == "d") {
        char charA[10];
        serialCommunication.get("v").toCharArray(charA, 10);
        float floatT = atof(charA);
        distance = floatT;
      }
      if (event == "o") {
        char charA[10];
        serialCommunication.get("v").toCharArray(charA, 10);
        float floatT = atof(charA);
        water = floatT;
      }
      if (event == "br") {
        while(1) {

        }
      }
    }
  }
  event = String();
}

