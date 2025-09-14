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

volatile bool pressed = false;
volatile unsigned long lastInterruptTime = 0;
volatile unsigned long lastLocalToggle = 0;

const unsigned long ISR_DEBOUNCE_MS = 50;
const unsigned long IGNORE_MQTT_AFTER_LOCAL_MS = 500;

char ssid[32];
char password[65];
char payload[84];

const char* toggleWaterTopic = "toggle/water";
const char* onOffWaterTopic = "on_off/water";
const char* getConfigTopic = "get_config/asker";
const char* keepAliveTopic = "keep/alive";

bool connected = false;
bool mqttConnected = false;
unsigned long pumpStart = 0;

AsyncMqttClient mqttClient;

void toggleWaterPump(bool newStatus) {
  enabledWaterPump = newStatus;
  digitalWrite(TX, enabledWaterPump ? LOW : HIGH);
  if (!enabledWaterPump) {
    pumpStart = 0;
  }
}

void publishStatus(bool status, const char* type) {
  sendItToServer = false;
  snprintf(payload, sizeof(payload), "%s:%s:%s:%s", secretKey, iam, type, (status ? "1" : "0"));
  mqttClient.publish(onOffWaterTopic, 0, false, payload);
}

void onMqttConnect(bool sessionPresent) {
  mqttConnected = true;
  mqttClient.subscribe(toggleWaterTopic, 0);
  mqttClient.subscribe(getConfigTopic, 0);
  if (sendItToServer) {
    publishStatus(valueToSend, "manual");
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  mqttConnected = false;
  toggleWaterPump(false);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {

  String msg;
  for (size_t i = 0; i < len; i++) msg += payload[i];

  if (strcmp(topic, toggleWaterTopic) == 0) {
    if (millis() - (unsigned long)lastLocalToggle < IGNORE_MQTT_AFTER_LOCAL_MS) {
      return;
    }
    toggleWaterPump(msg.toInt() ? true : false);
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

void IRAM_ATTR handleButton() {
  unsigned long now = millis();
  if (now - lastInterruptTime > ISR_DEBOUNCE_MS) {
    if (digitalRead(RX) == LOW) {
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
  attachInterrupt(digitalPinToInterrupt(RX), handleButton, FALLING);

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
    connected = false;
    toggleWaterPump(false);
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

  if (enabledWaterPump && pumpStart == 0) {
    pumpStart = millis();
  }
  if (enabledWaterPump && ((millis() - pumpStart) > maxEnabledTime * 60000UL)) {
    toggleWaterPump(false);
    pumpStart = 0;
    if (mqttConnected) {
      publishStatus(false, "auto");
    }
  }
  if (!enabledWaterPump) {
    pumpStart = 0;
  }

  /* button handling */

  if (pressed) {
    pressed = false;
    delay(10);
    if (digitalRead(RX) == LOW) {
      lastLocalToggle = millis();
      toggleWaterPump(!enabledWaterPump);
      if (mqttConnected) {
        publishStatus(enabledWaterPump, "manual");
      } else {
        sendItToServer = true;
        valueToSend = enabledWaterPump;
      }
    }    
  }
}