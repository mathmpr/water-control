#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <SimpleTimer.h>
#include "config.h"

#define LED_PIN 2
#define RX 3
#define TX 1
#define REAL_TX_PIN 1

WiFiEventHandler gotIpEventHandler;
WiFiEventHandler disconnectedEventHandler;

SimpleTimer keepAliveTimer;
SimpleTimer mqttConnectTimer;
SimpleTimer wifiConnectTimer;

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
bool connecting = false;
bool mqttConnected = false;
unsigned long pumpStart = 0;

AsyncMqttClient mqttClient;

void onLed() {
  digitalWrite(LED_PIN, LOW);
}

void offLed() {
  digitalWrite(LED_PIN, HIGH);
}

void print(const char* message) {
  if (TX != REAL_TX_PIN) {
    Serial.println(message);
  }
}

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
  print("MQTT On!");
  onLed();
  mqttConnected = true;
  mqttClient.subscribe(toggleWaterTopic, 0);
  mqttClient.subscribe(getConfigTopic, 0);
  if (sendItToServer) {
    publishStatus(valueToSend, "manual");
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  print("MQTT Off!");
  offLed();
  mqttConnected = false;
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {

  String msg;
  for (size_t i = 0; i < len; i++) msg += payload[i];

  print(topic);
  print(msg.c_str());

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
  if (connected || connecting) {
    return;
  }
  print("Try to connect!");
  connecting = true;
  bool found = false;
  int n = WiFi.scanNetworks();
  snprintf(payload, sizeof(payload), "Number of wifi's: %d", n);
  print(payload);
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
      yield();
    }  
  }
  WiFi.scanDelete();
  print("Delete scan");
  yield();
  connecting = false;
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

void connectMqtt() {
  if (connected && !mqttConnected) {
    mqttClient.connect();
  }
}

void publishKeepAlive() {
  if (mqttConnected) {
    snprintf(payload, sizeof(payload), "%s:%s", secretKey, iam);
    mqttClient.publish(keepAliveTopic, 0, false, payload);
  }
}

void setup() {

  pinMode(LED_PIN, OUTPUT);
  offLed();

  if (TX == REAL_TX_PIN) {
    pinMode(TX, OUTPUT);
    digitalWrite(TX, HIGH);
  } else {
    Serial.begin(9600);
    delay(1000);
  }

  print("Starting");

  pinMode(RX, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RX), handleButton, FALLING);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(mqttServer, mqttPort);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);

  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
    onLed();
    connected = true;
    connecting = false;
    print("Got IP");
  });

  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    offLed();
    connected = false;
    print("Disconnected");
  });

  keepAliveTimer.setInterval(8300, publishKeepAlive);
  mqttConnectTimer.setInterval(5700, connectMqtt);
  wifiConnectTimer.setInterval(12500, connectToWifi);

  connectToWifi();

  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
}

void loop() {

  keepAliveTimer.run();
  mqttConnectTimer.run();
  wifiConnectTimer.run();

  /* auto turn off handling */

  if (enabledWaterPump && pumpStart == 0) {
    pumpStart = millis();
  }
  if (enabledWaterPump && ((millis() - pumpStart) > (maxEnabledTime * 60000UL))) {
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

  ESP.wdtFeed();
}