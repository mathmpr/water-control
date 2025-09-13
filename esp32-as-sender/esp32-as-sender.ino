#include <WiFi.h>
#include <Arduino.h>
#include <Ticker.h>
#include "config.h"
#include "esp_task_wdt.h"
#include <AsyncMqttClient.h>

#define LED_BUILTIN 2

Ticker blinkTicker;
Ticker waterDetectTicker;
Ticker waterIncomeTicker;
Ticker keepAliveTicker;
Ticker mqttConnectTicker;
AsyncMqttClient mqtt;

char ssid[32];
char password[65];
char payload[85];

bool connected = false;
bool mqttConnected = false;
bool ledState = HIGH;
int Nsamples = 16;

const char* detectWaterTopic = "detect/water";
const char* incomeWaterTopic = "income/water";
const char* getConfigTopic = "get_config/sender";
const char* keepAliveTopic = "keep/alive";

const int waterDetectSensor = 34;
int sendDetectAt = 300;
long waterDetect;
volatile bool detectFlag = false;

const int waterIncomeSensor = 35;
int sendIncomeAt = 250;
long waterIncome;
volatile bool incomeFlag = false;

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

void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info)
{
  if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP){
    connected = true;
    blinkTicker.detach();
    digitalWrite(LED_BUILTIN, HIGH);
  } else if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
    connected = false;
    digitalWrite(LED_BUILTIN, LOW);
    connectToWifi();
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

void IRAM_ATTR detectWaterISR() {
  detectFlag = true;
}

void IRAM_ATTR incomeWaterISR() {
  incomeFlag = true;
}

void setup() {

  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  waterDetectTicker.attach(5, detectWaterISR);
  waterIncomeTicker.attach(17, incomeWaterISR);

  mqtt.setServer(mqttServer, mqttPort);
  mqtt.onConnect([](bool sessionPresent){
    mqttConnected = true;
    Serial.println("MQTT conectado!");
    mqtt.subscribe("blink/led", 0);
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

  mqtt.onDisconnect([](AsyncMqttClientDisconnectReason reason) {
    Serial.println("MQTT desconectado!");
    mqttConnected = false;
  });

  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWiFiEvent);

  connectToWifi();

  keepAliveTicker.attach(8, []() {
    if (mqttConnected) {
      snprintf(payload, sizeof(payload), "%s:%s", secretKey, iam);
      mqtt.publish(keepAliveTopic, 0, false, payload);
    }
  });

  mqttConnectTicker.attach(5, []() {
    if (connected && !mqttConnected) {
      connectToMqtt();
    }
  });

  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL); 
}

void loop() {
  if (detectFlag) {
    detectFlag = false;
    detectWater();
  }
  if (incomeFlag) {
    incomeFlag = false;
    incomeWater();
  }
  esp_task_wdt_reset();
}
