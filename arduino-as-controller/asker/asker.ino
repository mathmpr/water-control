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

LiquidCrystal_I2C lcd(I2C_ADDR, columns, lines);

int info = 0;
bool waiting = true;

const int relePin7 = 7;

unsigned long releInterval = 1000;
unsigned long limit = 0;
unsigned long limitCount = 0;
int minutes = 15;
bool firstConfig = false;

void getConfig() {
  if (!connected || !hasInternet) {
    return;
  }
  mapSender.add("event", "make-request");
  mapSender.add("web-event", "get-config");
  mapSender.add("value", "asker");
  send();
  if(!firstConfig) {
    firstConfig = true;
    setTimer("get-config", 1000 * 60 * 5);
  }
}

void makeRequest() {
  if (!connected || !hasInternet) {
    return;
  }
  if (limitCount >= limit) {
    mapSender.add("force-off", "off");
  }
  mapSender.add("event", "make-request");
  mapSender.add("web-event", "status");
  mapSender.add("iam", "asker");
  send();
}

void toogleInfo() {
  if (info == 0) {
    String message = String("Status: ") + (waiting ? "off" : (limitCount >= limit ? "safe" : "on"));
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

void controlRele() {
  if (!waiting && limitCount <= limit) {
    digitalWrite(relePin7, LOW);
    limitCount += releInterval;
  } else {
    digitalWrite(relePin7, HIGH);
  }
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

  limit = releInterval * 60 * minutes;

  lcd.init();
  
  lcd.backlight();
  lcd.noCursor();

  pinMode(relePin7, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(relePin7, HIGH);

  Serial.begin(9600);
  ss.begin(9600);
  delay(1000);

  setTimer("blink", 0, performBlink);
  setTimer("make-request", 5000, makeRequest);
  setTimer("info", 2000, toogleInfo);
  setTimer("get-config", 2000, getConfig);
  setTimer("connected", 2500, checkConnection);
  setTimer("rele", 1000, controlRele);
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
    } else if (event == "request-response") {
      if (mapReceiver.get("web-event") == "get-config") {
        minutes = mapReceiver.get("minutes").toInt();
        limit = releInterval * 60 * minutes;
      } else {
        waiting = !(mapReceiver.get("status") == "true");
        if (waiting) {
          limitCount = 0;
        }
      }
    } else if (event == "connection") {
      connected = mapReceiver.get("has-connection") == "true";
      hasInternet = mapReceiver.get("has-internet") == "true";
    }
  }
}