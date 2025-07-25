# Progetto di Tesi su ESP32-S3 Ai Camera Per EdgeAI


## Struttura del Progetto

```
ESP32CAM_ESPIDF/
├── CMakeLists.txt                 # Configurazione build principale
├── sdkconfig                      # Configurazione ESP-IDF generata
├── partitions.csv                 # Tabella partizioni custom (per modelli)
├── istruzioni.txt                 # Istruzioni d'uso o note
├── README.md                      # Questo file
├── main/                          # Applicazione principale
│   ├── CMakeLists.txt
│   ├── idf_component.yml          # Dipendenze gestite (incluso ESP-DL)
│   └── main.cpp                   # Entry point dell'applicazione
│
├── components/                    # Componenti custom del progetto
│   ├── camera/                    # Componente gestione fotocamera
│   │   ├── CMakeLists.txt
│   │   ├── camera.cpp             # Implementazione fotocamera
│   │   └── camera.h               # Header fotocamera
│   ├── webserver/                 # Componente webserver REST/HTTP
│   │   ├── CMakeLists.txt
│   │   ├── webserver.cpp          # Implementazione webserver
│   │   ├── webserver.h            # Header webserver
│   │   └── main_page.html         # Interfaccia web embedded
│   ├── inference/                 # Componente AI/inferenza
│   │   ├── CMakeLists.txt
│   │   ├── inference.cpp          # Logica di inferenza AI
│   │   └── include/
│   │       └── inference.h        # API per l'inferenza
│   └── monitor/                   # Sistema di monitoraggio risorse
│       ├── CMakeLists.txt
│       ├── monitor.cpp            # Implementazione monitoraggio
│       └── include/
│           └── monitor.h          # API per il monitoraggio
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
### Installazione ESP-IDF (fuori dalla repo)
```bash
#clona la repo di esp-idf
git clone https://github.com/espressif/esp-idf.git
cd esp-idf

#installa esp-idf per la specifica architettura
./install.sh esp32s3

#attiva ambiente di sviluppo (ora puoi effettuare comandi idf.py su questo terminale)
source export.sh 

#per avere l'ambiente di sviluppo sempre attivo, aggiungi questa riga al tuo .bashrc o .zshrc
#in questo modo, export.sh si attiva ad ogni nuovo terminale aperto
#NOTA: Assicurati che il percorso $HOME/esp/esp-idf/ sia quello corretto della tua installazione
source $HOME/esp/esp-idf/export.sh

```
- Python 3.8+

### Comandi di build (dentro la repo)
```bash
# Configurazione iniziale
idf.py set-target esp32s3

#Pulisci build
idf.py fullclean

#Installa tutte le dipendenze necessarie
idf.py reconfigure

# Compilazione
idf.py build

# Compilazione + Flash su dispositivo
idf.py -p /dev/tty.usbmodemXXXX flash

# Monitor seriale
idf.py -p /dev/tty.usbmodemXXXX monitor

# Per poter inserire input da tastiera in monitor seriale per CLI
idf.py menuconfig -> Component config → ESP System Settings → Channel for console output -> USB CDC

#Per poter caricare modelli e immagini ad alta risoluzione in RAM
idf.py menuconfig -> abilita PSIRAM/PSRAM (di default ci sarà solo DRAM)

#Se la memoria flash non basta (di default ci solo si 4mb utilizzabili dei 16gb disponibili)
idf.py menuconfig -> aumenta memoria flash utilizzabile da 4mb a 16mb

#Se hai problemi nel flashare il codice oppure nell'aprire il monitor
-> tieni premuto bottone BOOT, premi e rilascia RST, poi rilascia BOOT, sulla scheda(così entri in in modalità download). Poi flasha il firmware. Poi premi RST per uscire da modalità download. Ora puoi aprire monitor. Lancia due volte il comando del monitor nel caso.

#Per massime prestazioni della cpu
-> setta i 160 Mhz di default della CPU a 240 Mhz
```

### Configurazione WiFi
Modifica `main.cpp` per impostare le credenziali WiFi per il web server:
```cpp
#define WIFI_SSID "Tua_Rete_WiFi"
#define WIFI_PASS "Tua_Password"
```
IP per connettersi al webserver da browser: 172.20.10.3