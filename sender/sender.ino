#include <SoftwareSerial.h>

bool ledState = LOW;

SoftwareSerial ss(-1, 3);

const int waterSensorPin = A0;

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);
  ss.begin(9600);
}

void loop() {

  digitalWrite(LED_BUILTIN, HIGH);

  delay(2000);

  long status = (int)(analogRead(waterSensorPin) / 3);

  ss.print('<');
  ss.print(status);
  ss.println('>');

  digitalWrite(LED_BUILTIN, LOW);

  delay(2000);
}
