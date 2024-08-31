#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Wire.h>

bool ledState = LOW;

SoftwareSerial ss(8, 9);

#define I2C_ADDR 0x27
#define columns 16
#define lines 2

LiquidCrystal_I2C lcd(I2C_ADDR, columns, lines);

const int waterSensorPin = A0;

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.noCursor();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);
  ss.begin(9600);
  delay(1000);
}

char message[32];

const int times = 20;
int loop_c = 0;

void clearSoftwareSerialBuffer(SoftwareSerial &serial) {
  while (serial.available() > 0) {
    serial.read();
  }
}

long currentTime = 0;
long interval = 5000;

void loop() {

  clearSoftwareSerialBuffer(ss);

  digitalWrite(LED_BUILTIN, HIGH);

  long _millis = millis();
  if (_millis > interval + currentTime) {
    ss.println("<ev=pr&v=sender>");
    currentTime = _millis;
  }

  long status = (analogRead(waterSensorPin) / 3);

  if (status > 205) {
    snprintf(message, sizeof(message), "<ev=o&v=%d>", status);
    ss.println(message);
  }

  delay(300);

  clearSoftwareSerialBuffer(ss);

  lcd.setCursor(0, 0);
  snprintf(message, sizeof(message), "Status: %s", (status > 205 ? "water" : "air"));  
  lcd.print(message);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  snprintf(message, sizeof(message), "Looping: %d", loop_c);
  lcd.print(message);
  lcd.print("                ");

  delay(300);

  digitalWrite(LED_BUILTIN, LOW);

  delay(600);

  loop_c++;

  if(loop_c >= 10) {
    loop_c = 0;
  }
  
}
