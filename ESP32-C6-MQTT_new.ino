#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "SystemState.h"
#include "WaterMonitorMQTT.h"
#include "Notifier.h"
#include "PumpController.h"
#include "WebInterface.h"

// --- Obiekty globalne ---
Preferences preferences;
SystemState systemState;
WaterMonitorMQTT waterMQTT;
Notifier notifier(systemState);
PumpController pumpController(systemState, notifier);
WebInterface webInterface(systemState, waterMQTT, pumpController, preferences);

// --- Zmienne konfiguracyjne ---
String ssid, pass, pushoverToken, pushoverUser;
int sensorLowPin, sensorHighPin, sensorMidPin, relayPin, manualButtonPin;
bool isConfigured = false;

const char* apSSID = "ESP32-Setup";
const char* apPASS = "12345678";

// --- Watchdog ---
hw_timer_t *watchdogTimer = NULL;
void IRAM_ATTR resetModule() {
  ets_printf("Watchdog reboot\n");
  esp_restart();
}

void loadConfig() {
    preferences.begin("config", true);
    isConfigured = preferences.getBool("configured", false);
    if (isConfigured) {
        sensorLowPin = preferences.getInt("lowPin", 34);
        sensorHighPin = preferences.getInt("highPin", 35);
        sensorMidPin = preferences.getInt("midPin", -1);
        relayPin = preferences.getInt("relayPin", 25);
        manualButtonPin = preferences.getInt("buttonPin", -1);
        ssid = preferences.getString("ssid", "");
        pass = preferences.getString("pass", "");
        pushoverToken = preferences.getString("pushtoken", "");
        pushoverUser = preferences.getString("pushuser", "");
    }
    preferences.end();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Rozpoczęcie działania...");

    loadConfig();

    // Inicjalizacja Watchdoga
    watchdogTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(watchdogTimer, &resetModule, true);
    timerAlarmWrite(watchdogTimer, 15000000, false); // 15 sekund
    timerAlarmEnable(watchdogTimer);

    // Inicjalizacja modułów
    pumpController.begin(sensorLowPin, sensorHighPin, sensorMidPin, relayPin, manualButtonPin);
    notifier.begin(pushoverUser, pushoverToken);
    
    waterMQTT.begin(preferences);
    waterMQTT.setPins(sensorLowPin, sensorHighPin, sensorMidPin, relayPin);
    // Przekazujemy wskaźniki do zmiennych stanu w `systemState`
    waterMQTT.setWaterStates(&systemState.pumpOn, &systemState.manualMode, &systemState.testMode);

    if (!isConfigured) {
        WiFi.softAP(apSSID, apPASS);
        Serial.println("Tryb konfiguracyjny AP uruchomiony");
        webInterface.begin();
        return;
    }

    // Łączenie z WiFi
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        systemState.wifiConnected = true;
        Serial.println("\nPołączono z Wi-Fi. IP: " + WiFi.localIP().toString());
        systemState.addEvent("Połączono z Wi-Fi: " + WiFi.localIP().toString());
        notifier.sendPushover("Urządzenie online: " + WiFi.localIP().toString());
    } else {
        systemState.wifiConnected = false;
        WiFi.softAP("ESP32-WaterMonitor", "pompa123");
        Serial.println("\nNie udało się połączyć z WiFi, uruchomiono AP");
        systemState.addEvent("Tryb offline - AP");
    }

    // Inicjalizacja serwera WWW i mDNS
    webInterface.begin();
    if (MDNS.begin("esp32")) {
        Serial.println("mDNS uruchomiony jako esp32.local");
    } else {
        Serial.println("Błąd inicjalizacji mDNS!");
    }
}

void loop() {
    timerWrite(watchdogTimer, 0); // Reset watchdoga

    // Pętle głównych modułów
    pumpController.loop();
    webInterface.handleClient();
    waterMQTT.loop();

    // Sprawdzanie połączenia WiFi
    static unsigned long lastWifiCheck = 0;
    if (millis() - lastWifiCheck > 60000) {
        lastWifiCheck = millis();
        bool previousState = systemState.wifiConnected;
        systemState.wifiConnected = (WiFi.status() == WL_CONNECTED);
        if (systemState.wifiConnected && !previousState) {
            systemState.addEvent("Ponownie połączono z WiFi");
            notifier.sendPushover("Urządzenie ponownie online");
        } else if (!systemState.wifiConnected && previousState) {
            systemState.addEvent("Utracono połączenie WiFi");
        }
    }

    // Aktualizacja diody LED
    // (tę logikę również można przenieść do osobnej małej klasy lub funkcji)
    static unsigned long previousLedMillis = 0;
    static bool ledState = LOW;
    int statusLedPin = 2; // Można przenieść do konfiguracji
    if (!systemState.wifiConnected) {
        digitalWrite(statusLedPin, HIGH);
    } else {
        if (millis() - previousLedMillis >= 500) {
            previousLedMillis = millis();
            ledState = !ledState;
            digitalWrite(statusLedPin, ledState);
        }
    }
}
