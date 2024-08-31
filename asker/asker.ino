#include <SimpleMap.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <neotimer.h>

bool ledState = LOW;

SoftwareSerial ss(8, 9);

#define I2C_ADDR 0x27
#define columns 16
#define lines 2

LiquidCrystal_I2C lcd(I2C_ADDR, columns, lines);

int info = 0;
bool waiting = true;

const int relePin7 = 7;

char received[32];
char message[32];
int start;
int end;
bool status;
bool espResponse;
long offInterval;
long offIncrements;
long _minutes;
long _second;
int loop_c;
Neotimer offTimer = Neotimer(1000);
Neotimer logic = Neotimer(1000);
Neotimer lowLed = Neotimer(750);

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.noCursor();

  pinMode(relePin7, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(relePin7, HIGH);

  Serial.begin(9600);
  ss.begin(9600);
  delay(500);

  loop_c = 0;
  _second = 1000;
  _minutes = 10;
  offIncrements = 0;
  offInterval = 1000;
  espResponse = false;
  status = false;
}

void loop() {

  if (lowLed.repeat(NEOTIMER_UNLIMITED)) {

    digitalWrite(LED_BUILTIN, LOW);
  
  }

  if (logic.repeat(NEOTIMER_UNLIMITED)) {

    digitalWrite(LED_BUILTIN, HIGH);

    ss.println("<ev=pr&v=asker>");

    delay(50);

    if (ss.available()) {
      int i = 0;
      while (ss.available() && i < sizeof(received) - 1) {
        received[i++] = ss.read();
      }
      received[i] = '\0';
      char *startPtr = strchr(received, '<');
      char *endPtr = strchr(received, '>');
      if (startPtr && endPtr && endPtr > startPtr) {
        espResponse = true;
        start = startPtr - received;
        end = endPtr - received;
        if (strstr(received, "v=t")) {
          status = true;
        } else {
          status = false;
        }
      }
    }

    if (status) {
      digitalWrite(relePin7, LOW);
    } else {
      digitalWrite(relePin7, HIGH);
    }

    snprintf(message, sizeof(message), "Status: %s", status ? "on" : "off");
    lcd.setCursor(0, 0);
    lcd.print(message);
    lcd.print("              ");
    lcd.setCursor(0, 1);
    snprintf(message, sizeof(message), "Looping: %d", loop_c);
    lcd.print(message);
    lcd.print("              ");

    digitalWrite(LED_BUILTIN, LOW);

    loop_c++;

    if(loop_c >= 10) {
      loop_c = 0;
    }
  }

  if(offTimer.repeat(NEOTIMER_UNLIMITED)) {
    if (espResponse) {
      espResponse = false;
      offIncrements = 0;
      return;
    }
    if(status) {
      offIncrements += offInterval;
      if(offIncrements > (_second * 60 * _minutes)) {
        status = false;
      }
    } 
  }
  
}