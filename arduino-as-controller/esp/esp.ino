#include <SimpleMap.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

bool hasInternet = false;
bool connected = false;
const char* checkURL = "https://www.google.com";
const char* endpoint = "https://";
bool ledState = HIGH;
String responseString;
String fullUrl;

SoftwareSerial ss(3, 1);

char ssid[15];
char password[15];

const int CONNECTING  = 1000;
const int FAILED      = 500;
const int NO_INTERNET = 250;

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
SimpleMap mapSender;
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
  sender = mapSender.toUrlString();
  ss.println("<" + sender + ">");
}

void received() {
  event = String();
  if (ss.available()) {
    receive = ss.readStringUntil('\n');
    int start = receive.indexOf('<');
    int end = receive.indexOf('>');
    if (start > -1 && end > -1) {
      receive = receive.substring(start + 1, end);
      mapReceiver.fromUrlString(receive.c_str());
      event = mapReceiver.get("event");
    }
  } 
}

void needCredentials() {
  if (strlen(ssid) > 0) {
    setTimer("need-credentials", 0);
  } else {
    mapSender.clear();
    mapSender.add("event", "need-credentials");
    send();
  }
}

void makeRequest() {
  mapSender.clear();
  if (!connected) {    
    mapSender.add("event", "connection");
    mapSender.add("has-connection", "false");
    mapSender.add("has-internet", "false");
    send();
    return;
  }
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  fullUrl = String(endpoint) + mapReceiver.toUrlString();
  http.begin(client, fullUrl);
  int httpCode = http.GET();
  if (httpCode > 0) {
    mapSender.fromUrlString(http.getString().c_str());
    mapSender.add("response-code", String(httpCode).c_str());
    mapSender.add("event", "request-response");
    send();
  } else {
    mapSender.add("event", "connection");
    mapSender.add("has-connection", "true");
    mapSender.add("has-internet", "false");
    send();
  }
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
    clear();
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    hasInternet = false;
    setTimer("blink", NO_INTERNET);
  }
  send();
  http.end();
}

void checkConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    connected = true;
    clear();    
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    connected = false;
    setTimer("blink", FAILED);
  }
}

void connectToWifi() {
  if (strlen(ssid) == 0) {
    return;
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(9600);
  ss.begin(9600);
  delay(1000);

  setTimer("blink", 0, performBlink);
  blink(CONNECTING);
  setTimer("need-credentials", 2500, needCredentials);
}

void persistConnection() {
  checkConnection();
  checkInternet();
  mapSender.clear();
  mapSender.add("event", "connection");
  mapSender.add("has-connection", (connected ? "true" : "false"));
  mapSender.add("has-internet", (hasInternet ? "true" : "false"));
  send();
}

void loop() {
  if(Serial.available()) {
    ss.println(Serial.readStringUntil('\n'));
  }
  run();
  received();
  if (event.length() > 0) {
    if (event == "make-request") {
      makeRequest();
    }
    if (event == "credentials") {
      strncpy(ssid, mapReceiver.get("ssid").c_str(), sizeof(ssid));
      strncpy(password, mapReceiver.get("password").c_str(), sizeof(password));
      connectToWifi();
      persistConnection();
    }
    if (event == "has-connected") {
      if (!connected) {
        connectToWifi();
      }
      persistConnection();
    }

  }
}

