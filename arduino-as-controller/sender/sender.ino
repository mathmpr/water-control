#include <SimpleMap.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>

bool ledState = LOW;

bool hasInternet = false;
bool connected = false;


SoftwareSerial ss(8, 9);

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
  mapSender.clear();
}

void received() {
  event = String();
  if (ss.available()) {
    receive = ss.readStringUntil('\n');
    int start = receive.indexOf('<');
    int end = receive.indexOf('>');
    if (start > -1 && end > -1) {
      receive = receive.substring(start + 1, end);
      Serial.println(receive);
      mapReceiver.fromUrlString(receive.c_str());
      receive = String();
      event = mapReceiver.get("event");
    }
  } 
}

#define I2C_ADDR 0x27
#define columns 16
#define lines 2

#define pinEcho 2
#define pinTrig 3

LiquidCrystal_I2C lcd(I2C_ADDR, columns, lines);

volatile unsigned long beginPulse = 0;
volatile float distance = 0;
volatile int mode = -1;
int info = 0;

void mesureDistance(){
  switch (mode) {
    case 0: {
        beginPulse = micros();
        mode = 1;
        break;
      }
    case 1: {
        distance = (float)(micros() - beginPulse) / 58.3;
        beginPulse = 0;
        mode = -1;
        break;
      }
  }
}

int onDistance = 200;
int offDistance = 60;
bool firstConfig = false;

void getConfig() {
  if (!connected || !hasInternet) {
    return;
  }
  mapSender.add("event", "make-request");
  mapSender.add("web-event", "get-config");
  mapSender.add("value", "sender");
  send();
  if(!firstConfig) {
    firstConfig = true;
    setTimer("get-config", 1000 * 60 * 5);
  }
}

void sendPulse(){
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);
  mode = 0;
}

void makeRequest() {
  if (!connected || !hasInternet) {
    return;
  }
  mapSender.add("event", "make-request");
  mapSender.add("web-event", "turn-on");
  mapSender.add("distance", String(distance).c_str());
  mapSender.add("iam", "sender");
  if (distance > onDistance) {
    mapSender.add("value", "on");
  } else if(distance < offDistance) {
    mapSender.add("value", "off");
  }
  send();
}

void checkDistance() {
  sendPulse();
  delay(25);
}

void toogleInfo() {
  if (info == 0) {
    char tempStr[10];
    dtostrf((distance / 100), 6, 2, tempStr);
    String message = "Distance: ";
    message += tempStr;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(message);
    lcd.setCursor(0, 1);
    message = "Memory: " + String(freeMemory()) + "kb";
    lcd.print(message);
    info = 1;
  } else if(info == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    String statusMessage =  String("Connected: ") + (connected ? "Yes" : "No");
    lcd.print(statusMessage);
    lcd.setCursor(0, 1);
    statusMessage =  String("Internet: ") + (hasInternet ? "Yes" : "No");
    lcd.print(statusMessage);
    statusMessage = String();
    info = 0;
  }
}

#ifdef __arm__
extern "C" char* sbrk(int incr);
#else
extern char *__brkval;
#endif

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif
}

void reset() {
  wdt_enable(WDTO_15MS);
  while (1) {}
}

void checkConnection() {
  if(!connected || !hasInternet) {
    mapSender.add("event", "has-connected");
    send();
  }  
}

void setup() {
  lcd.init();
  
  lcd.backlight();
  lcd.noCursor();

  pinMode(pinEcho, INPUT);
  pinMode(pinTrig, OUTPUT);
  digitalWrite(pinTrig, LOW);
  attachInterrupt(digitalPinToInterrupt(pinEcho), mesureDistance, CHANGE);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);
  ss.begin(9600);
  delay(1000);

  setTimer("blink", 0, performBlink);
  setTimer("distance", 750, checkDistance);
  setTimer("make-request", 5000, makeRequest);
  setTimer("get-config", 2000, getConfig);
  setTimer("connected", 2500, checkConnection);
  setTimer("info", 2000, toogleInfo);
  setTimer("rele", 1000 * 60 * 30, reset);
  blink(500);
}

void loop() {
  if (freeMemory() < 250) {
    reset();
  }
  run();
  received();
  if (event.length() > 0) {
    if (event == "need-credentials") {
      mapSender.add("event", "credentials");
      mapSender.add("ssid", "");
      mapSender.add("password", "");
      send();
    } else if (event == "connection") {
      connected = mapReceiver.get("has-connection") == "true";
      hasInternet = mapReceiver.get("has-internet") == "true";
    } else if (event == "request-response") {
      if (mapReceiver.get("web-event") == "get-config") {
        onDistance = mapReceiver.get("on-distance").toInt();
        offDistance = mapReceiver.get("off-distance").toInt();
      }
    }
  }
  mapReceiver.clear();
}