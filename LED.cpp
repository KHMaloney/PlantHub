#include "LED.h"

LED::LED(int p) {
  pin = p;
  pinMode(pin, OUTPUT);
}

void LED::on() {
  digitalWrite(pin, HIGH);
}

void LED::off() {
  digitalWrite(pin, LOW);
}

void LED::fade(int value) {
  analogWrite(pin, constrain(value, 0, 255));
}
