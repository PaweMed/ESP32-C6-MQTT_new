#include "WebInterface.h"
#include <Update.h>
#include <ESPmDNS.h>

// Konstruktor: inicjalizuje referencje i obiekty
WebInterface::WebInterface(SystemState& state, WaterMonitorMQTT& mqtt, PumpController& pump, Preferences& prefs)
    : server(80),
      systemState(state),
      waterMQTT(mqtt),
      pumpController(pump),
      preferences(prefs) {
}

// Metoda do ładowania konfiguracji potrzebnej DLA interfejsu (piny, hasła itp.)
// To jest oddzielone od globalnego loadConfig, aby zachować hermetyzację
void WebInterface::loadLocalConfig() {
    preferences.begin("config", true);
    sensorLowPin = preferences.getInt("lowPin", 34);
    sensorHighPin = preferences.getInt("highPin", 35);
    sensorMidPin = preferences.getInt("midPin", -1);
    relayPin = preferences.getInt("relayPin", 25);
    manualButtonPin = preferences.getInt("buttonPin", -1);
    ssid = preferences.getString("ssid", "");
    pass = preferences.getString("pass", "");
    pushoverToken = preferences.getString("pushtoken", "");
    pushoverUser = preferences.getString("pushuser", "");
    preferences.end();
}

// Rejestracja wszystkich ścieżek (URL) serwera
void WebInterface::begin() {
    loadLocalConfig(); // Załaduj konfigurację pinów/haseł przy starcie serwera

    server.on("/", HTTP_GET, [this](){ this->handleStatus(); });
    server.on("/manual", HTTP_GET, [this](){ this->handleManual(); });
    server.on("/manual", HTTP_POST, [this](){ this->handleManual(); });
    server.on("/config", HTTP_GET, [this](){ this->handleConfigForm(); });
    server.on("/save", HTTP_POST, [this](){ this->handleSave(); });
    server.on("/log", HTTP_GET, [this](){ this->handleLog(); });
    server.on("/mqtt_config", HTTP_GET, [this](){ this->handleMQTTConfig(); });
    server.on("/save_mqtt", HTTP_GET, [this](){ this->handleSaveMQTT(); }); // Używamy GET, bo formularz wysyła GET
    
    // Obsługa aktualizacji OTA
    server.on("/update", HTTP_POST,
        [this](){
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
            ESP.restart();
        },
        [this](){ this->handleUpdateUpload(); }
    );

    server.begin();
}

void WebInterface::handleClient() {
    server.handleClient();
}

// --- Główne handlery stron ---

void WebInterface::handleStatus() {
    sendPage(); // Wyślij stronę główną bez dodatkowej zawartości
}

void WebInterface::handleLog() {
    String content = "<div class='control-panel'><h3><i class='fas fa-history'></i> Historia Zdarzeń</h3><ul style='list-style-type:none; padding-left:10px;'>";
    for (int i = 0; i < EVENT_LIMIT; i++) {
        int idx = (systemState.eventIndex + i) % EVENT_LIMIT;
        if (systemState.events[idx] != "") {
            content += "<li><i class='fas fa-angle-right' style='color:var(--primary); margin-right:5px;'></i>" + systemState.events[idx] + "</li>";
        }
    }
    content += "</ul></div>";
    sendPage(content);
}

void WebInterface::handleManual() {
    if (server.method() == HTTP_POST) {
        bool actionTaken = false;
        if (server.hasArg("toggle")) {
            pumpController.togglePumpManual();
            systemState.addEvent(String("Ręczne sterowanie POMPA (WWW) – ") + (systemState.pumpOn ? "WŁĄCZONA" : "WYŁĄCZONA"));
            // Powiadomienie Pushover jest wysyłane z poziomu PumpController
            actionTaken = true;
        } else if (server.hasArg("test")) {
            systemState.testMode = !systemState.testMode;
            if (systemState.testMode) {
                systemState.manualMode = true;
                systemState.manualModeStartTime = millis();
            } else {
                systemState.manualMode = false;
            }
            systemState.addEvent(systemState.testMode ? "Włączono tryb testowy" : "Wyłączono tryb testowy");
            actionTaken = true;
        } else if (server.hasArg("auto")) {
            systemState.manualMode = false;
            systemState.testMode = false; // Wyjście z trybu manualnego wyłącza też testowy
            systemState.addEvent("Przywrócono sterowanie automatyczne");
            actionTaken = true;
        }
        
        if(actionTaken) {
            server.sendHeader("Location", "/manual", true);
            server.send(303);
            return;
        }
    }

    // Generowanie HTML dla przycisków sterowania ręcznego
    String content = R"rawliteral(
    <div class="control-panel">
        <style> .btn { padding: 10px 15px; border: none; border-radius: 5px; color: white; cursor: pointer; text-decoration: none; display: inline-block; font-size: 16px; margin-top: 10px; width: 100%; text-align: center; } .btn-pump { background-color: var(--primary); } .btn-pump:hover { background-color: #2980b9; } .btn-danger { background-color: var(--danger); } .btn-danger:hover { background-color: #c0392b; } .btn-primary { background-color: var(--secondary); } .btn-primary:hover { background-color: #27ae60; } .btn-secondary { background-color: #7f8c8d; } .btn-secondary:hover { background-color: #6c7a7d; } .control-group { margin-bottom: 20px; } </style>
        <div class="control-group">
            <h3><i class="fas fa-cog"></i> Sterowanie Pompą</h3>
            <form method="POST" action="/manual" style="margin: 0;"><button type="submit" name="toggle" value="1" class="btn btn-pump"><i class="fas fa-power-off"></i> )rawliteral";
    content += systemState.pumpOn ? "WYŁĄCZ POMPĘ" : "WŁĄCZ POMPĘ";
    content += R"rawliteral(</button></form>
        </div>
        <div class="control-group">
            <h3><i class="fas fa-vial"></i> Tryb Testowy</h3>
            <form method="POST" action="/manual" style="margin: 0;"><button type="submit" name="test" value="1" class="btn )rawliteral";
    content += systemState.testMode ? "btn-danger" : "btn-primary";
    content += R"rawliteral("><i class="fas fa-flask"></i> )rawliteral";
    content += systemState.testMode ? "Wyłącz Tryb Testowy" : "Włącz Tryb Testowy";
    content += R"rawliteral(</button></form>
        </div>)rawliteral";

    if (systemState.manualMode || systemState.testMode) {
        content += R"rawliteral(
        <div class='control-group'>
            <h3><i class='fas fa-robot'></i> Sterowanie Automatyczne</h3>
            <form method='POST' action='/manual' style='margin: 0;'><button type='submit' name='auto' value='1' class='btn btn-secondary'><i class='fas fa-redo'></i> Przywróć Automat</button></form>
        </div>)rawliteral";
    }
    content += "</div>";
    
    sendPage(content);
}


void WebInterface::handleConfigForm() {
    String content = R"rawliteral(
    <div class="control-panel">
        <style> input { width: calc(100% - 10px); padding: 5px; } .btn-primary { padding: 10px 15px; border: none; border-radius: 5px; color: white; cursor: pointer; background-color: var(--primary); width: 100%; margin-top: 10px; } </style>
        <h3><i class="fas fa-sliders-h"></i> Konfiguracja</h3>
        <form action='/save' method='POST'>
            <label>Pin DOLNY:</label><br><input name='low' value=')rawliteral" + String(sensorLowPin) + R"rawliteral(' required><br><br>
            <label>Pin GÓRNY:</label><br><input name='high' value=')rawliteral" + String(sensorHighPin) + R"rawliteral(' required><br><br>
            <label>Pin ŚRODKOWY (wpisz -1, jeśli nieużywany):</label><br><input name='mid' value=')rawliteral" + String(sensorMidPin) + R"rawliteral('><br><br>
            <label>Pin przekaźnika:</label><br><input name='relay' value=')rawliteral" + String(relayPin) + R"rawliteral(' required><br><br>
            <label>Pin przycisku ręcznego (wpisz -1, jeśli nieużywany):</label><br><input name='button' value=')rawliteral" + String(manualButtonPin) + R"rawliteral('><br><br>
            <label>SSID Wi-Fi:</label><br><input name='ssid' value=')rawliteral" + ssid + R"rawliteral('><br><br>
            <label>Hasło Wi-Fi:</label><br><input type='password' name='pass' value=')rawliteral" + pass + R"rawliteral('><br><br>
            <label>Token Pushover:</label><br><input name='token' value=')rawliteral" + pushoverToken + R"rawliteral('><br><br>
            <label>Użytkownik Pushover:</label><br><input name='user' value=')rawliteral" + pushoverUser + R"rawliteral('><br><br>
            <input type='submit' class='btn btn-primary' value='Zapisz i zrestartuj'>
        </form>
    </div>)rawliteral";
    sendPage(content);
}

void WebInterface::handleSave() {
    preferences.begin("config", false);
    preferences.putInt("lowPin", server.arg("low").toInt());
    preferences.putInt("highPin", server.arg("high").toInt());
    preferences.putInt("midPin", server.arg("mid").toInt());
    preferences.putInt("relayPin", server.arg("relay").toInt());
    preferences.putInt("buttonPin", server.arg("button").toInt());
    preferences.putString("ssid", server.arg("ssid"));
    preferences.putString("pass", server.arg("pass"));
    preferences.putString("pushtoken", server.arg("token"));
    preferences.putString("pushuser", server.arg("user"));
    preferences.putBool("configured", true);
    preferences.end();
    
    String content = "<h3>Zapisano konfigurację. Restart za 3 sekundy...</h3>";
    sendPage(content);
    delay(3000);
    ESP.restart();
}

void WebInterface::handleMQTTConfig() {
    String content = R"rawliteral(
    <div class="control-panel">
      <style> input { width: calc(100% - 10px); padding: 5px; } .btn-primary { padding: 10px 15px; border: none; border-radius: 5px; color: white; cursor: pointer; background-color: var(--primary); width: 100%; margin-top: 10px; } </style>
      <h3><i class="fas fa-cloud"></i> Konfiguracja MQTT</h3>
      <form action='/save_mqtt' method='GET'>
        <label>Serwer MQTT:</label><br><input name='server' value=')rawliteral" + waterMQTT.getServer() + R"rawliteral('><br><br>
        <label>Port:</label><br><input name='port' type='number' value=')rawliteral" + String(waterMQTT.getPort()) + R"rawliteral('><br><br>
        <label>Użytkownik:</label><br><input name='user' value=')rawliteral" + waterMQTT.getUser() + R"rawliteral('><br><br>
        <label>Hasło:</label><br><input name='pass' type='password' value=')rawliteral" + waterMQTT.getPassword() + R"rawliteral('><br><br>
        <input type='submit' class='btn btn-primary' value='Zapisz'>
      </form>
    </div>)rawliteral";
    sendPage(content);
}

void WebInterface::handleSaveMQTT() {
    waterMQTT.setConfig(
        server.arg("server"),
        server.arg("port").toInt(),
        server.arg("user"),
        server.arg("pass")
    );
    waterMQTT.saveConfig(preferences);
    
    String content = "<h3>Zapisano konfigurację MQTT. Zmiany zostaną zastosowane przy następnym połączeniu.</h3>";
    sendPage(content);
}

// --- Obsługa OTA ---
void WebInterface::handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Rozpoczęcie aktualizacji: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("Aktualizacja zakończona: %u bajtów\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

// --- Główna funkcja do generowania i wysyłania strony ---
void WebInterface::sendPage(const String& content) {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.sendHeader("Content-Type", "text/html; charset=utf-8");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200);

    String chunk;

    // Nagłówek i style
    chunk = F(R"rawliteral(
    <!DOCTYPE html>
    <html lang="pl">
    <head>
      <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>System Zbiornika Wody</title>
      <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap" rel="stylesheet">
      <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css" rel="stylesheet">
      <style>
        :root { --primary: #3498db; --secondary: #2ecc71; --danger: #e74c3c; --warning: #f39c12; --dark: #2c3e50; --light: #ecf0f1; }
        body { font-family: 'Roboto', sans-serif; background-color: #f5f7fa; color: #333; margin: 0; padding: 20px; }
        .container { max-width: 1000px; margin: 0 auto; background: white; border-radius: 15px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); overflow: hidden; }
        header { background: linear-gradient(135deg, var(--primary), var(--dark)); color: white; padding: 20px; text-align: center; }
        .badge { display: inline-block; padding: 5px 10px; border-radius: 20px; font-size: 14px; margin-top: 10px; font-weight: 500; }
        .test-mode { background-color: var(--warning); color: white; }
        .manual-mode { background-color: var(--primary); color: white; }
        .dashboard { display: grid; grid-template-columns: 2fr 1fr; gap: 20px; padding: 20px; }
        @media (max-width: 768px) { .dashboard { grid-template-columns: 1fr; } }
        .tank-container { background: white; border-radius: 10px; padding: 20px; box-shadow: 0 3px 10px rgba(0,0,0,0.05); }
        .tank { position: relative; max-width: 300px; margin: 0 auto; width: 100%; height: 300px; background: #e0f2fe; border-radius: 5px; overflow: hidden; border: 3px solid #b3e0ff; }
        .water { position: absolute; bottom: 0; width: 100%; background: linear-gradient(to top, #3b82f6, #60a5fa); transition: height 0.5s ease; }
        .sensor { position: absolute; left: 10px; width: calc(100% - 20px); height: 3px; background: var(--dark); border-radius: 3px; }
        .sensor::after { content: ''; position: absolute; right: -15px; top: -5px; width: 10px; height: 10px; border-radius: 50%; }
        .sensor.high { top: 10%; }
        .sensor.mid { top: 35%; }
        .sensor.low { top: 70%; }
        .sensor-label { position: absolute; right: -80px; top: -10px; font-size: 14px; font-weight: 500; white-space: nowrap; }
        .water-percentage { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); font-size: 24px; font-weight: 700; color: rgba(255,255,255,0.8); text-shadow: 0 2px 4px rgba(0,0,0,0.3); }
        .status-indicator { display: flex; align-items: center; margin-bottom: 10px; }
        .status-dot { width: 12px; height: 12px; border-radius: 50%; margin-right: 10px; }
        .status-on { background-color: var(--secondary); }
        .status-off { background-color: var(--danger); }
        .nav { display: flex; justify-content: space-around; background: var(--light); padding: 15px; border-radius: 10px; margin-top: 20px; }
        .nav a { color: var(--dark); text-decoration: none; font-weight: 500; transition: color 0.3s; }
        .nav a:hover { color: var(--primary); }
        .control-panel { background: white; border-radius: 10px; padding: 20px; box-shadow: 0 3px 10px rgba(0,0,0,0.05); }
      </style>
    </head>
    <body><div class="container"><header><h1><i class="fas fa-tint"></i> System Zbiornika Wody</h1>
    )rawliteral");
    server.sendContent(chunk);
    
    // Status trybu pracy
    chunk = "";
    if (systemState.testMode) {
        chunk = F("<div class='badge test-mode'><i class='fas fa-flask'></i> Tryb testowy</div>");
    } else if (systemState.manualMode) {
        unsigned long remaining = (systemState.manualModeStartTime + systemState.manualModeTimeout - millis()) / 60000;
        chunk = "<div class='badge manual-mode'><i class='fas fa-hand-paper'></i> Tryb manualny (" + String(remaining) + " min)</div>";
    }
    server.sendContent(chunk);

    // Wizualizacja zbiornika
    chunk = F("</header><div class='dashboard'><div class='tank-container'><h2><i class='fas fa-water'></i> Wizualizacja Zbiornika</h2><div class='tank'>");
    server.sendContent(chunk);
    
    chunk = "<div class='water' style='height:" + String(systemState.waterLevel) + "%'><div class='water-percentage'>" + String(systemState.waterLevel) + "%</div></div>";
    server.sendContent(chunk);

    // Czujniki
    bool low = systemState.testMode || systemState.sensorLowState;
    bool high = systemState.testMode || systemState.sensorHighState;
    bool mid = (sensorMidPin != -1) ? (systemState.testMode || systemState.sensorMidState) : false;
    
    chunk = "<div class='sensor high' style='background:" + String(high ? "var(--secondary)" : "var(--danger)") + ";'><span class='sensor-label'>Górny: " + (high ? "Zanurzony" : "Suchy") + "</span></div>";
    if(sensorMidPin != -1) chunk += "<div class='sensor mid' style='background:" + String(mid ? "var(--secondary)" : "var(--danger)") + ";'><span class='sensor-label'>Środkowy: " + (mid ? "Zanurzony" : "Suchy") + "</span></div>";
    chunk += "<div class='sensor low' style='background:" + String(low ? "var(--secondary)" : "var(--danger)") + ";'><span class='sensor-label'>Dolny: " + (low ? "Zanurzony" : "Suchy") + "</span></div>";
    server.sendContent(chunk);

    // Prawa kolumna (zawartość i status)
    chunk = F("</div></div><div>");
    server.sendContent(chunk);
    
    server.sendContent(content); // Wstawienie dynamicznej zawartości (logi/formularze)
    
    chunk = F("<div class='control-panel' style='margin-top:20px;'><h3><i class='fas fa-info-circle'></i> Status Systemu</h3>");
    server.sendContent(chunk);

    chunk = "<div class='status-indicator'><div class='status-dot " + String(systemState.pumpOn ? "status-on" : "status-off") + "'></div><span>Pompa: " + (systemState.pumpOn ? "WŁĄCZONA" : "WYŁĄCZONA") + "</span></div>";
    chunk += "<div class='status-indicator'><div class='status-dot " + String(systemState.wifiConnected ? "status-on" : "status-off") + "'></div><span>WiFi: " + (systemState.wifiConnected ? "Podłączone" : "Rozłączone") + "</span></div>";
    chunk += "<div class='status-indicator'><div class='status-dot " + String(waterMQTT.isConnected() ? "status-on" : "status-off") + "'></div><span>MQTT: " + (waterMQTT.isConnected() ? "Połączony" : "Rozłączony") + "</span></div>";
    chunk += "<div class='status-indicator'><div class='status-dot " + String((pushoverToken != "" && pushoverUser != "") ? "status-on" : "status-off") + "'></div><span>Powiadomienia: " + ((pushoverToken != "" && pushoverUser != "") ? "Aktywne" : "Nieaktywne") + "</span></div>";
    server.sendContent(chunk);
    
    // Nawigacja i zamknięcie strony
    chunk = F(R"rawliteral(
        </div></div></div>
        <div class="nav">
            <a href="/"><i class="fas fa-home"></i> Strona Główna</a>
            <a href="/manual"><i class="fas fa-hand-paper"></i> Sterowanie</a>
            <a href="/config"><i class="fas fa-sliders-h"></i> Konfiguracja</a>
            <a href="/mqtt_config"><i class="fas fa-cloud"></i> MQTT</a>
            <a href="/log"><i class="fas fa-history"></i> Historia</a>
        </div>
    </div></body></html>
    )rawliteral");
    server.sendContent(chunk);

    // Finalizuj odpowiedź
    server.client().stop();
}
