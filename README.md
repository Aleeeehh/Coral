# ESP32-S3 AI CAMERA con ESP-IDF e FreeRTOS


## Struttura del Progetto

```
ESP32CAM_ESPIDF/
├── CMakeLists.txt                 # Configurazione build principale
├── sdkconfig                      # Configurazione ESP-IDF generata
├── sdkconfig.defaults             # Configurazioni di default
├── dependencies.lock              # Dipendenze bloccate
├── README.md                      # Questo file
├── istruzioni.txt                 # Istruzioni d'uso o note
├── main/                          # Applicazione principale
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.cpp                   # Entry point dell'applicazione
│
├── components/                    # Componenti custom del progetto
│   └── webserver/                 # Componente webserver
│       ├── CMakeLists.txt
│       ├── webserver.cpp          # Implementazione webserver
│       └── webserver.h            # Header webserver
│
├── managed_components/            # Dipendenze gestite da ESP-IDF
│   ├── espressif__esp32-camera/   # Driver fotocamera e sensori
│   └── espressif__esp_jpeg/       # Decoder JPEG
│
├── build/                         # Output di compilazione (generato)
└── .gitignore                     # File da ignorare in git
```


## Endpoint Disponibili Interfaccia Web
- `GET /` - Pagina principale con interfaccia web
- `GET /capture` - Scatta una nuova foto
- `GET /photo` - Visualizza l'ultima foto scattata
- `GET /status` - Status del sistema (JSON)


##  Compilazione e Flash

### Prerequisiti
- ESP-IDF v6.0 o superiore
- Python 3.8+

### Comandi di build
```bash
# Configurazione iniziale
idf.py set-target esp32s3

#Pulisci build
idf.py fullclean

#Installa dipendenze
idf.py reconfigure

# Compilazione
idf.py build

# Flash su dispositivo
idf.py -p /dev/tty.usbmodemXXXX flash

# Monitor seriale
idf.py -p /dev/tty.usbmodemXXXX monitor
```

### Configurazione WiFi
Modifica `main.cpp` per impostare le credenziali WiFi per il web server:
```cpp
#define WIFI_SSID "Tua_Rete_WiFi"
#define WIFI_PASS "Tua_Password"
```