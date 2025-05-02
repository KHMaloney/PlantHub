#ifndef LED_H
#define LED_H

#include <Arduino.h>

class LED {
  private:
    int pin;

  public:
    LED(int p);
    void on();
    void off();
    void fade(int value);
};

#endif
