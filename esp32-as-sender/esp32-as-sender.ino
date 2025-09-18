#include <WiFi.h>
#include <Arduino.h>
#include <SimpleTimer.h>
#include "config.h"
#include "esp_task_wdt.h"
#include <AsyncMqttClient.h>

#define LED_BUILTIN 2

AsyncMqttClient mqtt;

SimpleTimer waterDetectTimer;
SimpleTimer waterIncomeTimer;
SimpleTimer keepAliveTimer;
SimpleTimer mqttConnectTimer;
SimpleTimer wifiConnectTimer;

char ssid[32];
char password[65];
char payload[85];

bool connected = false;
bool connecting = false;
bool mqttConnected = false;
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

void offLed() {
  digitalWrite(LED_BUILTIN, LOW);
}

void onLed() {
  digitalWrite(LED_BUILTIN, HIGH);
}

void connectToMqtt() {
  if(connected){
    Serial.println("Conectando ao broker MQTT...");
    mqtt.connect();
  }
}

void disconnectMqtt() {
  if(mqttConnected) {
    Serial.println("Desconectando do broker MQTT...");
    mqtt.disconnect();
  } else {
    Serial.println("MQTT já desconectado.");
  }
}

void connectToWifi() {
  Serial.println("Im alive!");
  if (connected || connecting) {
    return;
  }
  connecting = true;
  bool found = false;
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
        yield();
      }
      if (found) {
        break;
      }
      yield();
    }  
  }
  yield();
  WiFi.scanDelete();
  connecting = false;
}

void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info)
{
  if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP){
    Serial.println("Got ip");
    connected = true;
    connecting = false;
    onLed();
  } else if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
    Serial.println("Disconnected");
    connected = false;
    offLed();
  }
}

int readAvg(int pin, int samples = Nsamples, int settle_ms = 8) {
  delay(settle_ms);
  long sum = 0;
  for (int i = 0; i < samples; ++i) {
    sum += analogRead(pin);
    ets_delay_us(2);
  }
  return (int)(sum / samples);
}

void detectWater()
{
  waterDetect = (readAvg(waterDetectSensor) / 4);
  Serial.print("Detect: ");
  Serial.println(waterDetect);
  if (waterDetect > sendDetectAt) {
    snprintf(payload, sizeof(payload), "%s:%s:%d", secretKey, iam, waterDetect);
    mqtt.publish(detectWaterTopic, 0, false, payload);
  }
}

void incomeWater() {
  waterIncome = (readAvg(waterIncomeSensor) / 4);
  Serial.print("Income: ");
  Serial.println(waterIncome);
  if (waterIncome > sendIncomeAt) {
    snprintf(payload, sizeof(payload), "%s:%s:%d", secretKey, iam, waterIncome);
    mqtt.publish(incomeWaterTopic, 0, false, payload);
  }
}

void connectMqttFn() {
  if (connected && !mqttConnected) {
    connectToMqtt();
  }
}

void keepAliveFn() {
  if (mqttConnected) {
    snprintf(payload, sizeof(payload), "%s:%s", secretKey, iam);
    mqtt.publish(keepAliveTopic, 0, false, payload);
  }
}

void setup() {

  Serial.begin(9600);

  delay(500);

  Serial.println("Starting");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  mqtt.setServer(mqttServer, mqttPort);
  mqtt.onConnect([](bool sessionPresent){
    mqttConnected = true;
    Serial.println("MQTT conectado!");
    onLed();
  });

  mqtt.onDisconnect([](AsyncMqttClientDisconnectReason reason) {
    Serial.println("MQTT desconectado!");
    mqttConnected = false;
    offLed();
  });

  mqtt.onMessage([]
    (char* topic,
    char* payload,
    AsyncMqttClientMessageProperties properties,
    size_t len,
    size_t index,
    size_t total)
  {
    String msg;
    for(size_t i=0;i<len;i++) msg += (char)payload[i];
    if (strcmp(topic, getConfigTopic) == 0) {
      int separator = msg.indexOf(':');
      if (separator > 0) {
        sendDetectAt = msg.substring(0, separator).toInt();
        sendIncomeAt = msg.substring(separator + 1).toInt();
      }
    }
  });

  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWiFiEvent);
  WiFi.disconnect();

  waterDetectTimer.setInterval(5000, detectWater);
  waterIncomeTimer.setInterval(16500, incomeWater);
  keepAliveTimer.setInterval(8300, keepAliveFn);
  mqttConnectTimer.setInterval(7450, connectMqttFn);
  wifiConnectTimer.setInterval(12500, connectToWifi);

  delay(500);

  connectToWifi();

  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL); 
}

void loop() {
  waterDetectTimer.run();
  waterIncomeTimer.run();
  keepAliveTimer.run();
  mqttConnectTimer.run();
  wifiConnectTimer.run();
  esp_task_wdt_reset();
}
