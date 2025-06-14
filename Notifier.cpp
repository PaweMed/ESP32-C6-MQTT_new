#include "Notifier.h"

Notifier::Notifier(SystemState& state) : systemState(state) {}

void Notifier::begin(String user, String token) {
    pushoverUser = user;
    pushoverToken = token;
}

void Notifier::sendPushover(String msg) {
    // Nie wysyłaj tego samego komunikatu częściej niż co 30 sekund
    if (msg == lastMessage && millis() - lastSendTime < 30000) {
        Serial.println("[Pushover] Pominięto duplikat wiadomości: " + msg);
        return;
    }

    Serial.println("[Pushover] Próba wysłania: " + msg);
    if (!systemState.wifiConnected) {
        Serial.println("[Pushover] Błąd: Brak połączenia WiFi");
        return;
    }
    if (pushoverToken == "" || pushoverUser == "") {
        Serial.println("[Pushover] Błąd: Brak tokenu lub użytkownika");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    client.setTimeout(10000);
    https.setTimeout(10000);

    String url = "https://api.pushover.net/1/messages.json";
    if (https.begin(client, url)) {
        https.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String postData = "token=" + pushoverToken +
                          "&user=" + pushoverUser +
                          "&message=" + urlEncode(msg) +
                          "&title=Zbiornik z wodą";
        
        int httpCode = https.POST(postData);
        if (httpCode == HTTP_CODE_OK) {
            Serial.println("[Pushover] Wysłano pomyślnie!");
            lastMessage = msg;
            lastSendTime = millis();
        } else {
            Serial.println("[Pushover] Błąd wysyłania! HTTP Code: " + String(httpCode));
            Serial.println(https.getString());
        }
        https.end();
    } else {
        Serial.println("[Pushover] Błąd początkowania połączenia");
    }
}

String Notifier::urlEncode(String str) {
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (unsigned int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encodedString += '+';
        } else if (isalnum(c)) {
            encodedString += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) code0 = c - 10 + 'A';
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
    }
    return encodedString;
}
