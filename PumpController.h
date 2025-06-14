#ifndef PUMP_CONTROLLER_H
#define PUMP_CONTROLLER_H

#include <Arduino.h>
#include "SystemState.h"
#include "Notifier.h"

class PumpController {
public:
    PumpController(SystemState& state, Notifier& notifier);
    void begin(int lowPin, int highPin, int midPin, int relayPin, int buttonPin);
    void loop();
    void togglePumpManual();

private:
    void readSensors();
    void handleAutoControl();
    void handleManualButton();
    bool canTogglePump(bool manualOverride = false);

    SystemState& systemState;
    Notifier& notifier;

    // Piny
    int sensorLowPin, sensorHighPin, sensorMidPin, relayPin, manualButtonPin;

    // Zabezpieczenia
    unsigned long lastPumpToggleTime = 0;
    const unsigned long minPumpToggleInterval = 30000;
    int pumpToggleCount = 0;
    const int maxPumpTogglesPerMinute = 4;
    unsigned long lastMinuteCheck = 0;

    // Debouncing czujników
    unsigned long lastSensorChangeTime = 0;
    const unsigned long sensorDebounceTime = 5000;

    // Debouncing przycisku
    bool lastButtonState = HIGH;
    unsigned long lastButtonDebounceTime = 0;
    const unsigned long buttonDebounceDelay = 50;
    unsigned long lastButtonPressTime = 0;
    const unsigned long buttonPressDelay = 1000;
};

#endif
