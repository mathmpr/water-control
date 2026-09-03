#include <Arduino.h>
#include <MiniCore/HttpFirmwareSource.h>
#include <MiniCore/MiniCore.h>
#include <SimpleTimer.h>
#include <WiFi.h>
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_task_wdt.h"

#include "config.h"

#define LED_BUILTIN 2

SimpleTimer waterDetectTimer;
SimpleTimer waterIncomeTimer;
SimpleTimer keepAliveTimer;
SimpleTimer mqttConnectTimer;
SimpleTimer wifiConnectTimer;

MiniCore::ArduinoWifiHandler wifi;
MiniCore::ArduinoMqttClient mqttClient;
MiniCore::HttpFirmwareSource firmwareSource(MANIFEST_URL, FAMILY_ID);
MiniCore::Esp32OtaDriver otaDriver;
MiniCore::RemoteUpdater updater(firmwareSource, otaDriver, CURRENT_VERSION);
MiniCore::ArduinoPersistentStorage storage;

char payload[128];

bool connected = false;
bool connecting = false;
bool mqttConnected = false;
bool otaBlockedByVersionMismatch = false;
int Nsamples = 16;

const char* detectWaterTopic = "detect/water";
const char* incomeWaterTopic = "income/water";
const char* getConfigTopic = "get_config/sender";
const char* keepAliveTopic = "keep/alive";

const int waterDetectSensor = 34;
int sendDetectAt = 300;
long waterDetect;

const int waterIncomeSensor = 35;
int sendIncomeAt = 250;
long waterIncome;

unsigned long lastUpdateCheckMs = 0;
unsigned long lastWifiConnectAttemptMs = 0;

void print(const char* message) {
  Serial.println(message);
}

void offLed() {
  digitalWrite(LED_BUILTIN, LOW);
}

void onLed() {
  digitalWrite(LED_BUILTIN, HIGH);
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

void connectToWifi() {
  if (connected) {
    return;
  }

  if (connecting && millis() - lastWifiConnectAttemptMs < WIFI_CONNECT_TIMEOUT_MS) {
    return;
  }

  if (connecting) {
    print("WiFi connect timeout.");
    WiFi.disconnect();
    connecting = false;
  }

  if (connecting) {
    return;
  }

  print("Try to connect WiFi.");
  connecting = true;
  lastWifiConnectAttemptMs = millis();

  const int networks = wifi.scanNetworks();
  snprintf(payload, sizeof(payload), "Number of wifi's: %d", networks);
  print(payload);

  bool found = false;
  const char* selectedSsid = nullptr;
  const char* selectedPassword = nullptr;
  if (networks > 0) {
    for (int networkIndex = 0; networkIndex < networks; ++networkIndex) {
      const String scannedSsid = wifi.scannedSsid(networkIndex);
      Serial.print("SSID: ");
      Serial.println(scannedSsid);
      for (int credentialIndex = 0; credentialIndex < size; credentialIndex++) {
        if (scannedSsid == credentials[credentialIndex].ssid) {
          selectedSsid = credentials[credentialIndex].ssid;
          selectedPassword = credentials[credentialIndex].password;
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

  if (found && selectedSsid != nullptr) {
    Serial.print("Connecting to SSID: ");
    Serial.println(selectedSsid);
    wifi.connectStation(selectedSsid, selectedPassword);
    lastWifiConnectAttemptMs = millis();
  } else {
    print("No configured SSID found.");
    connecting = false;
  }

  yield();
}

void onMqttConnect() {
  mqttConnected = true;
  print("MQTT On!");
  mqttClient.subscribe(getConfigTopic, 0);
  onLed();
}

void onMqttDisconnect() {
  mqttConnected = false;
  print("MQTT Off!");
  offLed();
}

void onMqttMessage(const String& topic, const uint8_t* data, size_t len) {
  String msg;
  msg.reserve(len);
  for (size_t i = 0; i < len; i++) {
    msg += static_cast<char>(data[i]);
  }

  if (topic == getConfigTopic) {
    const int separator = msg.indexOf(':');
    if (separator > 0) {
      sendDetectAt = msg.substring(0, separator).toInt();
      sendIncomeAt = msg.substring(separator + 1).toInt();
    }
  }
}

void connectMqtt() {
  if (!connected || mqttConnected) {
    return;
  }

  print("Connecting MQTT.");
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

int readAvg(int pin, int samples = Nsamples, int settle_ms = 8) {
  delay(settle_ms);
  long sum = 0;
  for (int i = 0; i < samples; ++i) {
    sum += analogRead(pin);
    ets_delay_us(2);
  }
  return static_cast<int>(sum / samples);
}

void detectWater() {
  waterDetect = readAvg(waterDetectSensor) / 4;
  Serial.print("Detect: ");
  Serial.println(waterDetect);
  if (waterDetect > sendDetectAt && mqttClient.isConnected()) {
    snprintf(payload, sizeof(payload), "%s:%s:%ld", secretKey, iam, waterDetect);
    mqttClient.publish(detectWaterTopic, payload, 0, false);
  }
}

void incomeWater() {
  waterIncome = readAvg(waterIncomeSensor) / 4;
  Serial.print("Income: ");
  Serial.println(waterIncome);
  if (waterIncome > sendIncomeAt && mqttClient.isConnected()) {
    snprintf(payload, sizeof(payload), "%s:%s:%ld", secretKey, iam, waterIncome);
    mqttClient.publish(incomeWaterTopic, payload, 0, false);
  }
}

void keepAlive() {
  if (mqttClient.isConnected()) {
    snprintf(payload, sizeof(payload), "%s:%s", secretKey, iam);
    mqttClient.publish(keepAliveTopic, payload, 0, false);
  }
}

void keepRuntimeAliveDuringOta(size_t, size_t) {
  wifi.handle();
  mqttClient.handle();
  esp_task_wdt_reset();
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

void configureWatchdog() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_task_wdt_config_t config = {};
  config.timeout_ms = 10000;
  config.idle_core_mask = (1 << portNUM_PROCESSORS) - 1;
  config.trigger_panic = true;
  esp_err_t status = esp_task_wdt_init(&config);
  if (status == ESP_ERR_INVALID_STATE) {
    status = esp_task_wdt_reconfigure(&config);
  }
#else
  esp_task_wdt_init(10, true);
#endif
  if (esp_task_wdt_status(NULL) != ESP_OK) {
    esp_task_wdt_add(NULL);
  }
}

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(LED_BUILTIN, OUTPUT);
  offLed();

  storage.begin("water_ota");
  validatePendingOtaState();

  print("Starting");

  mqttClient.configure(mqttServer, mqttPort);
  mqttClient.onMessage(onMqttMessage);
  updater.onProgress(keepRuntimeAliveDuringOta);

  wifi.setMode(MiniCore::WifiMode::Station);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);

  wifi.onGotIp([](IPAddress) {
    connected = true;
    connecting = false;
    onLed();
    print("Got IP");
  });

  wifi.onDisconnected([]() {
    connected = false;
    connecting = false;
    onMqttDisconnect();
    print("Disconnected");
  });

  waterDetectTimer.setInterval(5000, detectWater);
  waterIncomeTimer.setInterval(16500, incomeWater);
  keepAliveTimer.setInterval(8300, keepAlive);
  mqttConnectTimer.setInterval(7450, connectMqtt);
  wifiConnectTimer.setInterval(12500, connectToWifi);

  connectToWifi();
  waitForInitialWifi(10000);
  checkForUpdates();
  lastUpdateCheckMs = millis();

  configureWatchdog();
}

void loop() {
  wifi.handle();
  syncMqttState();

  waterDetectTimer.run();
  waterIncomeTimer.run();
  keepAliveTimer.run();
  mqttConnectTimer.run();
  wifiConnectTimer.run();
  checkForUpdatesLoop();

  esp_task_wdt_reset();
}
