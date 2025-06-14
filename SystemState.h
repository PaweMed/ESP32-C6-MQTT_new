#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>

#define EVENT_LIMIT 20

struct SystemState {
    // Stan sprzętowy
    bool pumpOn = false;
    bool sensorLowState = false;
    bool sensorMidState = false;
    bool sensorHighState = false;
    int waterLevel = 0;

    // Tryby pracy
    bool manualMode = false;
    bool testMode = false;
    unsigned long manualModeStartTime = 0;
    const unsigned long manualModeTimeout = 30 * 60 * 1000; // 30 minut

    // Stan połączeń
    bool wifiConnected = false;

    // Zdarzenia
    String events[EVENT_LIMIT];
    int eventIndex = 0;

    void addEvent(String msg) {
        Serial.println(msg);
        events[eventIndex] = msg;
        eventIndex = (eventIndex + 1) % EVENT_LIMIT;
    }
};

#endif
