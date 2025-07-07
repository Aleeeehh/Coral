# Progetto di Tesi su ESP32-S3 Ai Camera Per EdgeAI


## Struttura del Progetto

```
ESP32CAM_ESPIDF/
├── CMakeLists.txt                 # Configurazione build principale
├── sdkconfig                      # Configurazione ESP-IDF generata
├── sdkconfig.defaults             # Configurazioni di default
├── partitions.csv                 # Tabella partizioni custom (per modelli)
├── istruzioni.txt                 # Istruzioni d'uso o note
├── README.md                      # Questo file
├── main/                          # Applicazione principale
│   ├── CMakeLists.txt
│   ├── idf_component.yml          # Dipendenze gestite (incluso ESP-DL)
│   └── main.cpp                   # Entry point dell'applicazione
│
├── components/                    # Componenti custom del progetto
│   ├── webserver/                 # Componente webserver REST/HTTP
│   │   ├── CMakeLists.txt
│   │   ├── webserver.cpp          # Implementazione webserver
│   │   ├── webserver.h            # Header webserver
│   │   └── main_page.html         # Interfaccia web embedded
│   └── inference/                 # Componente AI/inferenza
│       ├── CMakeLists.txt
│       ├── inference.cpp          # Logica di inferenza AI
│       └── include/
│           └── inference.h        # API per l'inferenza
│
├── managed_components/            # Componenti/dipendenze gestiti da ESP-IDF (ESP-DL, ecc)
├── build/                         # Output di compilazione (generato)
└── .gitignore                     # Files da ignorare per git
```


## Endpoint Disponibili Interfaccia Web
- `GET /` - Pagina principale con interfaccia web
- `GET /capture` - Scatta una nuova foto
- `GET /photo` - Visualizza l'ultima foto scattata
- `POST /inference` - Esegue inferenza AI per rilevamento facce (MSRMNP_S8_V1)


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