#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <WebServer.h>
#include <Preferences.h>
#include "SystemState.h"
#include "WaterMonitorMQTT.h"
#include "PumpController.h"

class WebInterface {
public:
    WebInterface(SystemState& state, WaterMonitorMQTT& mqtt, PumpController& pump, Preferences& prefs);
    void begin();
    void handleClient();

private:
    void handleStatus();
    void handleManual();
    void handleConfigForm();
    void handleSave();
    void handleLog();
    void handleMQTTConfig();
    void handleSaveMQTT();
    void handleUpdate();
    void handleUpdateUpload();

    void sendPage(String content = "");

    WebServer server;
    SystemState& systemState;
    WaterMonitorMQTT& waterMQTT;
    PumpController& pumpController;
    Preferences& preferences;
    
    // Zmienne konfiguracyjne, które nie są częścią stanu 'live'
    String ssid, pass, pushoverToken, pushoverUser;
    int sensorLowPin, sensorHighPin, sensorMidPin, relayPin, manualButtonPin;
};

#endif
