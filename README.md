# ESP32-CAM con ESP-IDF e FreeRTOS

Progetto ESP32-CAM migrato a ESP-IDF con FreeRTOS.


## 📁 Struttura del Progetto

```
ESP32CAM_ESPIDF/
├── CMakeLists.txt                 # Configurazione build principale
├── sdkconfig                      # Configurazione ESP-IDF
├── sdkconfig.defaults             # Configurazioni di default
├── dependencies.lock              # Dipendenze bloccate
├── README.md                      # Questo file
├── .gitignore                     # File da ignorare in git
│
├── main/                          # Applicazione principale
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.c                     # Entry point dell'applicazione
│
├── components/                    # Componenti modulari
│   ├── webserver/                 # Componente webserver
│   │   ├── CMakeLists.txt
│   │   ├── webserver.c            # Implementazione webserver
│   │   └── webserver.h            # Header webserver
│   │
│   └── led_control/               # Componente controllo LED
│       ├── CMakeLists.txt
│       ├── led_control.c          # Implementazione LED
│       └── led_control.h          # Header LED
│
└── managed_components/            # Dipendenze gestite da ESP-IDF
    ├── espressif__esp32-camera/   # Driver fotocamera
    └── espressif__esp_jpeg/       # Decoder JPEG
```


### Endpoint Disponibili Interfaccia Web
- `GET /` - Pagina principale con interfaccia web
- `GET /capture` - Scatta una nuova foto
- `GET /photo` - Visualizza l'ultima foto scattata
- `GET /status` - Status del sistema (JSON)
ß
## 🛠️ Compilazione e Flash

### Prerequisiti
- ESP-IDF v6.0 o superiore
- Python 3.8+

### Comandi Build
```bash
# Configurazione iniziale
idf.py set-target esp32

# Compilazione
idf.py build

# Flash su dispositivo
idf.py -p /dev/tty.usbserial-XXXX flash

# Monitor seriale
idf.py -p /dev/tty.usbserial-XXXX monitor
```

### Configurazione WiFi
Modifica `main.c` per impostare le credenziali WiFi:
```c
#define WIFI_SSID "Tua_Rete_WiFi"
#define WIFI_PASS "Tua_Password"
```