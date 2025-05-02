#include "Plant.h"

Plant::Plant(String name, int mstPin_, int dhtPin, int prPin_, int redPin, int greenPin, int bluePin)
  : plantName(name), mstPin(mstPin_), dht(dhtPin, DHT11), prPin(prPin_),
    redLed(redPin), greenLed(greenPin), blueLed(bluePin),
    currentR(0), currentG(0), currentB(0),
    targetR(0), targetG(0), targetB(0), lastFadeUpdate(0) {}

void Plant::begin() {
  dht.begin();
  forceLed(128, 128, 128);
}

void Plant::forceLed(int r, int g, int b) {
  currentR = r;
  currentG = g;
  currentB = b;
  redLed.fade(r);
  greenLed.fade(g);
  blueLed.fade(b);
}

float Plant::getTemperatureF() {
  return dht.readTemperature(true);
}

float Plant::getHumidity() {
  return dht.readHumidity();
}

int Plant::getLightLevel() {
  int prValue = analogRead(prPin);
  return map(prValue, 4095, 0, 0, 100);
}

float Plant::getSoilMoisture() {
  int soilValue = analogRead(mstPin);
  return (1.0 - (soilValue / 4095.0)) * 100.0;
}

String Plant::getName() {
  return plantName;
}

void Plant::setName(String newName) {
  plantName = newName;
}

void Plant::setLed(int r, int g, int b) {
  targetR = constrain(r, 0, 255);
  targetG = constrain(g, 0, 255);
  targetB = constrain(b, 0, 255);
}

void Plant::updateFade() {
  unsigned long now = millis();
  if (now - lastFadeUpdate >= fadeInterval) {
    lastFadeUpdate = now;
    bool changed = false;

    if (currentR != targetR) {
      currentR += (targetR > currentR) ? 1 : -1;
      changed = true;
    }
    if (currentG != targetG) {
      currentG += (targetG > currentG) ? 1 : -1;
      changed = true;
    }
    if (currentB != targetB) {
      currentB += (targetB > currentB) ? 1 : -1;
      changed = true;
    }

    if (changed) {
      redLed.fade(currentR);
      greenLed.fade(currentG);
      blueLed.fade(currentB);
    }
  }
}

void Plant::updateLed(float tempF, float humidity, int lightLevel, float soilMoisture) {
  if (soilMoisture < 15) {
    setLed(255, 165, 0);
  } else if (tempF < 60) {
    setLed(0, 0, 255);
  } else if (tempF > 85) {
    setLed(255, 0, 0);
  } else if (humidity < 40) {
    setLed(255, 255, 0);
  } else if (lightLevel < 30) {
    setLed(128, 0, 128);
  } else {
    setLed(0, 255, 0);
  }
}
