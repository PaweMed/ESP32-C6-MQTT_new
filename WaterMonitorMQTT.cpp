#include "WaterMonitorMQTT.h"

WaterMonitorMQTT::WaterMonitorMQTT() : 
    mqttClient(espClient), 
    mqttPort(1883),
    mqttClientId("esp32-water-monitor"),
    mqttBaseTopic("homeassistant/sensor/water_monitor/"),
    lastReconnectAttempt(0),
    lastDataSend(0) {
}

void WaterMonitorMQTT::begin(Preferences& prefs) {
    loadConfig(prefs);
    if (!mqttServer.isEmpty()) {
        mqttClient.setServer(mqttServer.c_str(), mqttPort);
        mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->mqttCallback(topic, payload, length);
        });
    }
}

void WaterMonitorMQTT::setPins(int lowPin, int highPin, int midPin, int relayPin) {
    sensorLowPin = lowPin;
    sensorHighPin = highPin;
    sensorMidPin = midPin;
    relayControlPin = relayPin;
}

void WaterMonitorMQTT::setWaterStates(bool* pumpOn, bool* manualMode, bool* testMode) {
    pumpState = pumpOn;
    manualModeState = manualMode;
    testModeState = testMode;
}

void WaterMonitorMQTT::loadConfig(Preferences& prefs) {
    prefs.begin("mqtt", true);
    mqttServer = prefs.getString("server", "");
    mqttPort = prefs.getInt("port", 1883);
    mqttUser = prefs.getString("user", "");
    mqttPassword = prefs.getString("pass", "");
    prefs.end();
}

void WaterMonitorMQTT::saveConfig(Preferences& prefs) {
    prefs.begin("mqtt", false);
    prefs.putString("server", mqttServer);
    prefs.putInt("port", mqttPort);
    prefs.putString("user", mqttUser);
    prefs.putString("pass", mqttPassword);
    prefs.end();
}

void WaterMonitorMQTT::reconnect() {
    if (millis() - lastReconnectAttempt < 5000) return;
    
    lastReconnectAttempt = millis();
    Serial.println("Attempting MQTT connection...");
    
    if (mqttClient.connect(mqttClientId.c_str(), mqttUser.c_str(), mqttPassword.c_str())) {
        Serial.println("Connected to MQTT broker");
        // Subskrypcja tematów jeśli potrzebne
        mqttClient.subscribe((mqttBaseTopic + "pump/set").c_str());
    } else {
        Serial.print("Failed, rc=");
        Serial.print(mqttClient.state());
        Serial.println(" try again in 5 seconds");
    }
}

void WaterMonitorMQTT::mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    String topicStr(topic);
    if (topicStr == mqttBaseTopic + "pump/set") {
        if (message == "ON") {
            digitalWrite(relayControlPin, HIGH);
            *pumpState = true;
        } else if (message == "OFF") {
            digitalWrite(relayControlPin, LOW);
            *pumpState = false;
        }
    }
}

void WaterMonitorMQTT::sendData() {
    if (!mqttClient.connected()) return;
    
    bool low = digitalRead(sensorLowPin) == LOW;
    bool high = digitalRead(sensorHighPin) == LOW;
    bool mid = (sensorMidPin != -1) ? (digitalRead(sensorMidPin) == LOW) : false;
    
    int waterLevel = 0;
    if (high) waterLevel = 100;
    else if (mid) waterLevel = 65;
    else if (low) waterLevel = 30;
    else waterLevel = 5;

    mqttClient.publish((mqttBaseTopic + "level").c_str(), String(waterLevel).c_str());
    mqttClient.publish((mqttBaseTopic + "pump").c_str(), *pumpState ? "ON" : "OFF");
    mqttClient.publish((mqttBaseTopic + "mode").c_str(), *manualModeState ? "manual" : (*testModeState ? "test" : "auto"));
    mqttClient.publish((mqttBaseTopic + "low_sensor").c_str(), low ? "WET" : "DRY");
    mqttClient.publish((mqttBaseTopic + "high_sensor").c_str(), high ? "WET" : "DRY");
    if (sensorMidPin != -1) {
        mqttClient.publish((mqttBaseTopic + "mid_sensor").c_str(), mid ? "WET" : "DRY");
    }
}

void WaterMonitorMQTT::loop() {
    if (mqttServer.isEmpty()) return;
    
    if (!mqttClient.connected()) {
        reconnect();
    } else {
        mqttClient.loop();
        
        if (millis() - lastDataSend > 10000) { // Co 10 sekund
            sendData();
            lastDataSend = millis();
        }
    }
}