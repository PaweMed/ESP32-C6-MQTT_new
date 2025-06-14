#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "SystemState.h"

class Notifier {
public:
    Notifier(SystemState& state);
    void begin(String user, String token);
    void sendPushover(String msg);

private:
    String urlEncode(String str);
    SystemState& systemState;
    String pushoverUser;
    String pushoverToken;
    String lastMessage;
    unsigned long lastSendTime = 0;
};

#endif
