#include "PumpController.h"

PumpController::PumpController(SystemState& state, Notifier& notifier)
    : systemState(state), notifier(notifier) {}

void PumpController::begin(int lowPin, int highPin, int midPin, int relayPin, int buttonPin) {
    this->sensorLowPin = lowPin;
    this->sensorHighPin = highPin;
    this->sensorMidPin = midPin;
    this->relayPin = relayPin;
    this->manualButtonPin = buttonPin;

    pinMode(sensorLowPin, INPUT);
    pinMode(sensorHighPin, INPUT);
    if (sensorMidPin != -1) pinMode(sensorMidPin, INPUT);
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);

    if (manualButtonPin != -1) {
        pinMode(manualButtonPin, INPUT_PULLUP);
        lastButtonState = digitalRead(manualButtonPin);
    }
}

void PumpController::loop() {
    readSensors();
    if (manualButtonPin != -1) {
        handleManualButton();
    }
    handleAutoControl();
    
    // Sprawdzenie timeoutu dla trybu ręcznego
    if (systemState.manualMode && !systemState.testMode && (millis() - systemState.manualModeStartTime > systemState.manualModeTimeout)) {
        systemState.manualMode = false;
        systemState.addEvent("Automatyczne wyłączenie trybu manualnego po 30 minutach");
    }
}

void PumpController::readSensors() {
    bool currentLow = digitalRead(sensorLowPin) == LOW;
    bool currentHigh = digitalRead(sensorHighPin) == LOW;
    bool currentMid = (sensorMidPin != -1) ? (digitalRead(sensorMidPin) == LOW) : false;

    if (currentLow != systemState.sensorLowState || currentHigh != systemState.sensorHighState || currentMid != systemState.sensorMidState) {
        lastSensorChangeTime = millis();
    }
    
    systemState.sensorLowState = currentLow;
    systemState.sensorHighState = currentHigh;
    systemState.sensorMidState = currentMid;

    if (systemState.testMode) {
        systemState.sensorLowState = true;
        systemState.sensorHighState = true;
        systemState.sensorMidState = (sensorMidPin != -1);
    }

    if (systemState.sensorHighState) systemState.waterLevel = 100;
    else if (systemState.sensorMidState) systemState.waterLevel = 65;
    else if (systemState.sensorLowState) systemState.waterLevel = 30;
    else systemState.waterLevel = 5;
}

void PumpController::handleAutoControl() {
    if (systemState.manualMode || systemState.testMode) return;

    if (millis() - lastSensorChangeTime > sensorDebounceTime) {
        if (systemState.sensorHighState && systemState.pumpOn && canTogglePump()) {
            digitalWrite(relayPin, LOW);
            systemState.pumpOn = false;
            lastPumpToggleTime = millis();
            pumpToggleCount++;
            systemState.addEvent("Automatyczne wyłączenie pompy (górny czujnik)");
            notifier.sendPushover("Pompa została automatycznie wyłączona - zbiornik pełny");
        } else if (!systemState.sensorLowState && !systemState.pumpOn && canTogglePump()) {
            digitalWrite(relayPin, HIGH);
            systemState.pumpOn = true;
            lastPumpToggleTime = millis();
            pumpToggleCount++;
            systemState.addEvent("Automatyczne włączenie pompy (brak wody)");
            notifier.sendPushover("Pompa została automatycznie włączona - niski poziom wody");
        }
    }
}

void PumpController::handleManualButton() {
    bool reading = digitalRead(manualButtonPin);
    if (reading != lastButtonState) {
        lastButtonDebounceTime = millis();
    }

    if ((millis() - lastButtonDebounceTime) > buttonDebounceDelay) {
        if (reading != digitalRead(manualButtonPin)) { // Double check
             lastButtonState = reading;
             if (reading == LOW) { // Przycisk naciśnięty
                 if (millis() - lastButtonPressTime > buttonPressDelay) {
                    lastButtonPressTime = millis();
                    togglePumpManual();
                    systemState.addEvent(String("Przycisk BOOT POMPA – ") + (systemState.pumpOn ? "WŁĄCZONA" : "WYŁĄCZONA"));
                    notifier.sendPushover(String("Przycisk BOOT POMPA: ") + (systemState.pumpOn ? "włączono" : "wyłączono"));
                 }
             }
        }
    }
    lastButtonState = reading;
}


void PumpController::togglePumpManual() {
    if (canTogglePump(true)) {
        systemState.manualMode = true;
        systemState.manualModeStartTime = millis();
        systemState.pumpOn = !systemState.pumpOn;
        digitalWrite(relayPin, systemState.pumpOn ? HIGH : LOW);
        lastPumpToggleTime = millis();
        pumpToggleCount++;
    }
}


bool PumpController::canTogglePump(bool manualOverride) {
    if (manualOverride) return true;

    unsigned long now = millis();
    if (now - lastMinuteCheck > 60000) {
        pumpToggleCount = 0;
        lastMinuteCheck = now;
    }

    if (pumpToggleCount >= maxPumpTogglesPerMinute) {
        systemState.addEvent("Osiągnięto limit przełączeń pompy (4/min)");
        notifier.sendPushover("Osiągnięto limit przełączeń pompy (4/min) - bezpiecznik");
        return false;
    }

    if (now - lastPumpToggleTime < minPumpToggleInterval) {
        systemState.addEvent("Zbyt częste przełączanie pompy - bezpiecznik");
        notifier.sendPushover("Zbyt częste przełączanie pompy - bezpiecznik");
        return false;
    }
    return true;
}
