#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <Ticker.h>
#include "config.h"

#define LED_PIN 2
#define RX 3
#define TX 1

WiFiEventHandler gotIpEventHandler;
WiFiEventHandler disconnectedEventHandler;

Ticker keepAliveTicker;
Ticker mqttConnectTicker;

bool enabledWaterPump = false;
bool sendItToServer = false;
bool valueToSend = false;
int maxEnabledTime = 20;
int passedTimeInEnable = 0;

char ssid[32];
char password[65];
char payload[84];

const char* toggleWaterTopic = "toggle/water";
const char* onOffWaterTopic = "on_off/water";
const char* getConfigTopic = "get_config/asker";
const char* keepAliveTopic = "keep/alive";

bool connected = false;
bool mqttConnected = false;

AsyncMqttClient mqttClient;

void onMqttConnect(bool sessionPresent) {
  mqttConnected = true;
  mqttClient.subscribe(toggleWaterTopic, 0);
  mqttClient.subscribe(getConfigTopic, 0);
  if (sendItToServer) {
    sendItToServer = false;
    snprintf(payload, sizeof(payload), "%s:%s:manual:%s", secretKey, iam, (valueToSend ? "1" : "0"));
    mqttClient.publish(onOffWaterTopic, 0, false, payload);
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  mqttConnected = false;
  digitalWrite(TX, HIGH);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {
  String msg;
  for (size_t i = 0; i < len; i++) msg += payload[i];
  if (strcmp(topic, toggleWaterTopic) == 0) {
    enabledWaterPump = msg.toInt() ? true : false;
    digitalWrite(TX, enabledWaterPump ? LOW : HIGH);
  } else if (strcmp(topic, getConfigTopic) == 0) {
    maxEnabledTime = msg.toInt();
  }
}

void connectToWifi() {
  if (connected) {
    return;
  }
  bool found = false;
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  if (n != 0) {
    for (int i = 0; i < n; ++i) {
      for(int j = 0; j < size; j++) {
        strncpy(ssid, credentials[j].ssid, sizeof(ssid));
        strncpy(password, credentials[j].password, sizeof(password));
        if (WiFi.SSID(i) == ssid) {
          WiFi.begin(ssid, password);
          found = true;
          break;
        }
      }
      if (found) {
        break;
      }
    }  
  }
  WiFi.scanDelete();
}

volatile bool pressed = false;
volatile unsigned long lastInterruptTime = 0;

void IRAM_ATTR handleButton() {
  if (digitalRead(RX) == LOW) {
    unsigned long now = millis();
    if (now - lastInterruptTime > 200) {
      pressed = true;
      lastInterruptTime = now;
    }
  }
}

void setup() {

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(TX, OUTPUT);
  digitalWrite(TX, HIGH);

  pinMode(RX, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RX), handleButton, CHANGE);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(mqttServer, mqttPort);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
    digitalWrite(LED_PIN, LOW);
    connected = true;
  });

  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(TX, HIGH);
    connected = false;
  });

  keepAliveTicker.attach(8, []() {
    if (mqttConnected) {
      snprintf(payload, sizeof(payload), "%s:%s", secretKey, iam);
      mqttClient.publish(keepAliveTopic, 0, false, payload);
    }    
  });

  mqttConnectTicker.attach(5, []() {
    if (connected && !mqttConnected) {
      mqttClient.connect();
    }
  });

  connectToWifi();

  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
}

void loop() {

  /* auto turn off handling */

  static unsigned long pumpStart = 0;
  if (enabledWaterPump && pumpStart == 0) {
    pumpStart = millis();
  }
  if (enabledWaterPump && ((millis() - pumpStart) > maxEnabledTime * 60000UL)) {
    enabledWaterPump = false;
    digitalWrite(TX, HIGH);
    pumpStart = 0;
    if (mqttConnected) {
      snprintf(payload, sizeof(payload), "%s:%s:auto:0", secretKey, iam);
      mqttClient.publish(onOffWaterTopic, 0, false, payload);
    }
  }
  if (!enabledWaterPump) {
    pumpStart = 0;
  }

  /* button handling */

  if (pressed) {
    pressed = false;
    enabledWaterPump = !enabledWaterPump;
    digitalWrite(TX, enabledWaterPump ? LOW : HIGH);
    if (mqttConnected) {
      snprintf(payload, sizeof(payload), "%s:%s:manual:%s", secretKey, iam, (enabledWaterPump ? "1" : "0"));
      mqttClient.publish(onOffWaterTopic, 0, false, payload);
    } else {
      sendItToServer = true;
      valueToSend = enabledWaterPump;
    }
  }
}