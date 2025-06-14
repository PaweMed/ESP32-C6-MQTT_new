#ifndef WATER_MONITOR_MQTT_H
#define WATER_MONITOR_MQTT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>

class WaterMonitorMQTT {
public:
    WaterMonitorMQTT();
    void begin(Preferences& prefs);
    void setPins(int lowPin, int highPin, int midPin, int relayPin);
    void setWaterStates(bool* pumpOn, bool* manualMode, bool* testMode);
    void loop();
    void sendData();
    bool isConnected() { return mqttClient.connected(); }

    String getServer() const { return mqttServer; }
    int getPort() const { return mqttPort; }
    String getUser() const { return mqttUser; }
    String getPassword() const { return mqttPassword; }
    
    void setConfig(String server, int port, String user, String pass) {
        mqttServer = server;
        mqttPort = port;
        mqttUser = user;
        mqttPassword = pass;
    }

    void saveConfig(Preferences& prefs);

private:
    void reconnect();
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    void loadConfig(Preferences& prefs);

    WiFiClient espClient;
    PubSubClient mqttClient;

    String mqttServer;
    int mqttPort;
    String mqttUser;
    String mqttPassword;
    String mqttClientId;
    String mqttBaseTopic;

    // Referencje do stanu z głównego programu
    bool* pumpState;
    bool* manualModeState;
    bool* testModeState;
    
    // Piny czujników
    int sensorLowPin;
    int sensorHighPin;
    int sensorMidPin;
    int relayControlPin;

    unsigned long lastReconnectAttempt;
    unsigned long lastDataSend;
};

#endif