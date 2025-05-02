#ifndef PLANT_H
#define PLANT_H

#include <Arduino.h>
#include <DHT.h>
#include "LED.h"

class Plant {
  private:
    String plantName;
    int mstPin;
    int prPin;
    DHT dht;
    LED redLed, greenLed, blueLed;
    int currentR, currentG, currentB;
    int targetR, targetG, targetB;
    unsigned long lastFadeUpdate;
    const int fadeInterval = 20;

    void forceLed(int r, int g, int b);

  public:
    Plant(String name, int mstPin, int dhtPin, int prPin, int redPin, int greenPin, int bluePin);
    void begin();
    float getTemperatureF();
    float getHumidity();
    int getLightLevel();
    float getSoilMoisture();
    String getName();
    void setName(String newName);
    void setLed(int r, int g, int b);
    void updateFade();
    void updateLed(float tempF, float humidity, int lightLevel, float soilMoisture);
};

#endif
