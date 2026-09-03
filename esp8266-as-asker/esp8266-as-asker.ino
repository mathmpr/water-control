#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <MiniCore/HttpFirmwareSource.h>
#include <MiniCore/MiniCore.h>
#include <SimpleTimer.h>

#include "config.h"

#define LED_PIN 2
#define RX 3
#define TX 1
#define REAL_TX_PIN 1

constexpr bool SERIAL_DEBUG = false;

SimpleTimer keepAliveTimer;
SimpleTimer mqttConnectTimer;
SimpleTimer wifiConnectTimer;

MiniCore::ArduinoWifiHandler wifi;
MiniCore::ArduinoMqttClient mqttClient;
MiniCore::HttpFirmwareSource firmwareSource(MANIFEST_URL, FAMILY_ID);
MiniCore::Esp8266OtaDriver otaDriver;
MiniCore::RemoteUpdater updater(firmwareSource, otaDriver, CURRENT_VERSION);
MiniCore::ArduinoPersistentStorage storage;

bool enabledWaterPump = false;
bool sendItToServer = false;
bool valueToSend = false;
int maxEnabledTime = 20;
int numberOfFailedKeepAlives = 0;
int maxFailedKeepAlives = 30;

volatile bool pressed = false;
volatile unsigned long lastInterruptTime = 0;
volatile unsigned long lastLocalToggle = 0;

const unsigned long ISR_DEBOUNCE_MS = 50;
const unsigned long IGNORE_MQTT_AFTER_LOCAL_MS = 500;

char payload[128];

const char* toggleWaterTopic = "toggle/water";
const char* onOffWaterTopic = "on_off/water";
const char* getConfigTopic = "get_config/asker";
const char* keepAliveTopic = "keep/alive";

bool connected = false;
bool connecting = false;
bool mqttConnected = false;
bool otaBlockedByVersionMismatch = false;
unsigned long pumpStart = 0;
unsigned long lastUpdateCheckMs = 0;

void onLed() {
  digitalWrite(LED_PIN, LOW);
}

void offLed() {
  digitalWrite(LED_PIN, HIGH);
}

int relayLevel(bool enabled) {
  if (relayActiveLow) {
    return enabled ? LOW : HIGH;
  }
  return enabled ? HIGH : LOW;
}

void print(const char* message) {
  if (SERIAL_DEBUG) {
    Serial.println(message);
  }
}

void validatePendingOtaState() {
  String pendingVersion;
  if (!storage.loadString(PENDING_OTA_VERSION_KEY, pendingVersion)) {
    return;
  }

  if (pendingVersion == CURRENT_VERSION) {
    storage.remove(PENDING_OTA_VERSION_KEY);
    print("OTA version confirmed.");
    return;
  }

  otaBlockedByVersionMismatch = true;
  print("OTA version mismatch after reboot. OTA disabled to avoid update loop.");
  snprintf(payload, sizeof(payload), "Current: %s Pending: %s", CURRENT_VERSION, pendingVersion.c_str());
  print(payload);
}

void toggleWaterPump(bool newStatus) {
  print("pump action trigger");
  enabledWaterPump = newStatus;
  if (!(SERIAL_DEBUG && TX == REAL_TX_PIN)) {
    digitalWrite(TX, relayLevel(enabledWaterPump));
  }
  if (!enabledWaterPump) {
    pumpStart = 0;
  }
}

void publishStatus(bool status, const char* type) {
  sendItToServer = false;
  snprintf(payload, sizeof(payload), "%s:%s:%s:%s", secretKey, iam, type, (status ? "1" : "0"));
  mqttClient.publish(onOffWaterTopic, payload, 0, false);
}

void onMqttConnect() {
  print("MQTT On!");
  onLed();
  mqttConnected = true;
  mqttClient.subscribe(toggleWaterTopic, 0);
  mqttClient.subscribe(getConfigTopic, 0);
  if (sendItToServer) {
    publishStatus(valueToSend, "manual");
  }
}

void onMqttDisconnect() {
  print("MQTT Off!");
  offLed();
  mqttConnected = false;
}

void onMqttMessage(const String& topic, const uint8_t* data, size_t len) {
  String msg;
  msg.reserve(len);
  for (size_t i = 0; i < len; i++) {
    msg += static_cast<char>(data[i]);
  }

  print(topic.c_str());
  print(msg.c_str());

  if (topic == toggleWaterTopic) {
    if (millis() - static_cast<unsigned long>(lastLocalToggle) < IGNORE_MQTT_AFTER_LOCAL_MS) {
      return;
    }
    toggleWaterPump(msg.toInt() ? true : false);
  } else if (topic == getConfigTopic) {
    maxEnabledTime = msg.toInt();
  }
}

void connectToWifi() {
  if (connected || connecting) {
    return;
  }

  print("Try to connect!");
  connecting = true;

  const int networks = wifi.scanNetworks();
  snprintf(payload, sizeof(payload), "Number of wifi's: %d", networks);
  print(payload);

  bool found = false;
  if (networks > 0) {
    for (int networkIndex = 0; networkIndex < networks; ++networkIndex) {
      const String scannedSsid = wifi.scannedSsid(networkIndex);
      for (int credentialIndex = 0; credentialIndex < size; credentialIndex++) {
        if (scannedSsid == credentials[credentialIndex].ssid) {
          wifi.connectStation(credentials[credentialIndex].ssid, credentials[credentialIndex].password);
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

  wifi.clearScanResults();
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
  if (!connected || mqttConnected) {
    return;
  }

  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    onMqttConnect();
  }
}

void syncMqttState() {
  mqttClient.handle();
  if (mqttConnected && !mqttClient.isConnected()) {
    onMqttDisconnect();
  }
}

void publishKeepAlive() {
  if (mqttClient.isConnected()) {
    snprintf(payload, sizeof(payload), "%s:%s", secretKey, iam);
    mqttClient.publish(keepAliveTopic, payload, 0, false);
    numberOfFailedKeepAlives = 0;
  } else {
    numberOfFailedKeepAlives++;
  }
  if (numberOfFailedKeepAlives > maxFailedKeepAlives) {
    ESP.restart();
  }
}

void keepRuntimeAliveDuringOta(size_t, size_t) {
  wifi.handle();
  mqttClient.handle();
  ESP.wdtFeed();
  delay(1);
}

void checkForUpdates() {
  if (!connected || otaBlockedByVersionMismatch) {
    return;
  }

  MiniCore::OtaResult result = updater.update();
  print("OTA result:");
  Serial.print(MiniCore::toString(result.error));
  Serial.print(" - ");
  Serial.println(result.message);

  if (result.decision == MiniCore::OtaDecision::Updated) {
    storage.saveString(PENDING_OTA_VERSION_KEY, result.toVersion);
    print("Restarting into new firmware.");
    delay(250);
    ESP.restart();
  }
}

void checkForUpdatesLoop() {
  const unsigned long now = millis();
  if (now - lastUpdateCheckMs < UPDATE_CHECK_INTERVAL_MS) {
    return;
  }

  lastUpdateCheckMs = now;
  checkForUpdates();
}

void waitForInitialWifi(unsigned long timeoutMs) {
  const unsigned long startedAt = millis();
  while (!connected && millis() - startedAt < timeoutMs) {
    wifi.handle();
    yield();
    delay(50);
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  offLed();

  if (SERIAL_DEBUG) {
    Serial.begin(9600);
    delay(1000);
  }
  storage.begin("water_ota");
  validatePendingOtaState();

  if (!(SERIAL_DEBUG && TX == REAL_TX_PIN)) {
    pinMode(TX, OUTPUT);
    digitalWrite(TX, relayLevel(false));
  }

  print("Starting");

  pinMode(RX, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RX), handleButton, FALLING);

  mqttClient.configure(mqttServer, mqttPort);
  mqttClient.onMessage(onMqttMessage);
  updater.onProgress(keepRuntimeAliveDuringOta);

  wifi.setMode(MiniCore::WifiMode::Station);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);

  wifi.onGotIp([](IPAddress) {
    onLed();
    connected = true;
    connecting = false;
    print("Got IP");
  });

  wifi.onDisconnected([]() {
    offLed();
    connected = false;
    connecting = false;
    print("Disconnected");
  });

  keepAliveTimer.setInterval(8300, publishKeepAlive);
  mqttConnectTimer.setInterval(5700, connectMqtt);
  wifiConnectTimer.setInterval(12500, connectToWifi);

  connectToWifi();
  waitForInitialWifi(10000);
  checkForUpdates();
  lastUpdateCheckMs = millis();

  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
}

void loop() {
  wifi.handle();
  syncMqttState();

  keepAliveTimer.run();
  mqttConnectTimer.run();
  wifiConnectTimer.run();
  checkForUpdatesLoop();

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
