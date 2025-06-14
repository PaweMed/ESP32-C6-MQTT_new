ESP32 Water Monitor - System Monitorowania Zbiornika Wody
Status Systemu

📌 Opis Projektu
System monitorowania poziomu wody w zbiorniku z możliwością:
Automatycznego sterowania pompą
Ręcznego nadpisania automatyki
Powiadomień Pushover
Zarządzania przez interfejs WWW
Aktualizacji OTA

🛠 Wymagane Komponenty
ESP32 
Czujniki poziomu wody (3x)
Przekaźnik sterujący pompą
Zasilanie 5V/12V
Połączenie WiFi


⚙ Konfiguracja
Pierwsze uruchomienie:
Po włączeniu urządzenie utworzy punkt dostępowy ESP32-Setup (hasło: 12345678)
Połącz się i przejdź do http://192.168.4.1
Wprowadź dane konfiguracyjne:
Piny czujników
Dane WiFi
Token i użytkownik Pushover
Domyślne dane logowania:
SSID: ESP32-Setup
Hasło: 12345678


🔄 Tryby Pracy
Automatyczny:
Pompa włącza się gdy poziom wody spadnie poniżej czujnika dolnego
Pompa wyłącza się gdy woda osiągnie czujnik górny

Ręczny:
Naciśnij przycisk "POMPA" w interfejsie
Tryb wygasa po 30 minutach
Możliwość przywrócenia automatyki

Testowy:
Symuluje zanurzenie czujników
Aktywowany w /manual

🔔 Powiadomienia Pushover
System wysyła powiadomienia o:

Zmianie statusu pompy

Przełączeniu trybów pracy

Utracie/ponownym połączeniu z WiFi


📦 Struktura Kodu
├── Konfiguracja
│   ├── loadConfig()
│   └── saveConfig()
├── Interfejs WWW
│   ├── handleStatus()
│   ├── handleManual()
│   └── handleConfig()
├── Logika Pompy
│   ├── Automatyczne sterowanie
│   └── Ręczne sterowanie
└── Powiadomienia
    └── sendPushover()


📞 Wsparcie
W przypadku problemów:

Sprawdź logi przez Serial Monitor (115200 baud)

Skonsultuj się z dokumentacją komponentów

Sprawdź połączenia elektryczne

✏ Autor: [PaweMed]
   witkowski.med@gmail.com
📅 Wersja: 1.0
🔗 Licencja: MIT
