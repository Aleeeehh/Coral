# ESP32CAM con ESP-IDF e FreeRTOS

Questo progetto è la conversione del progetto ESP32CAM da Arduino IDE al framework ESP-IDF con FreeRTOS.

## 🚀 Caratteristiche

- **Framework ESP-IDF**: Utilizza il framework ufficiale di Espressif
- **FreeRTOS**: Sistema operativo real-time per gestione multi-task
- **Fotocamera ESP32CAM**: Supporto completo per ESP32CAM AI-Thinker
- **Webserver HTTP**: Interfaccia web moderna e responsive
- **Streaming MJPEG**: Streaming video in tempo reale
- **Scatto foto**: Acquisizione e visualizzazione foto
- **Controllo LED**: Gestione LED flash integrata
- **WiFi**: Connessione automatica con riconnessione
- **Thread-safe**: Utilizzo di mutex e semafori per accesso sicuro

## 📁 Struttura del Progetto

```
ESP32CAM_ESPIDF/
├── CMakeLists.txt              # Configurazione principale CMake
├── sdkconfig.defaults          # Configurazione predefinita ESP32CAM
├── main/                       # Applicazione principale
│   ├── CMakeLists.txt
│   └── main.c                  # Entry point con gestione task FreeRTOS
├── components/                 # Componenti modulari
│   ├── camera/                 # Gestione fotocamera
│   │   ├── CMakeLists.txt
│   │   ├── camera.h
│   │   └── camera.c
│   ├── webserver/              # Server HTTP
│   │   ├── CMakeLists.txt
│   │   ├── webserver.h
│   │   └── webserver.c
│   └── led_control/            # Controllo LED
│       ├── CMakeLists.txt
│       ├── led_control.h
│       └── led_control.c
└── README.md                   # Questa documentazione
```

## 🔧 Configurazione Hardware

### ESP32CAM AI-Thinker Pinout

| Pin | Funzione | Descrizione |
|-----|----------|-------------|
| 4   | LED Flash | LED flash controllabile |
| 5   | D0        | Data 0 fotocamera |
| 18  | D1        | Data 1 fotocamera |
| 19  | D2        | Data 2 fotocamera |
| 21  | D3        | Data 3 fotocamera |
| 36  | D4        | Data 4 fotocamera |
| 39  | D5        | Data 5 fotocamera |
| 34  | D6        | Data 6 fotocamera |
| 35  | D7        | Data 7 fotocamera |
| 0   | XCLK      | Clock fotocamera |
| 22  | PCLK      | Pixel clock |
| 25  | VSYNC     | Vertical sync |
| 23  | HREF      | Horizontal reference |
| 26  | SIOD      | I2C data |
| 27  | SIOC      | I2C clock |
| 32  | PWDN      | Power down |
| -1  | RESET     | Reset (non utilizzato) |

## 🛠️ Installazione e Compilazione

### Prerequisiti

1. **ESP-IDF**: Installare ESP-IDF v4.4 o superiore
   ```bash
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh
   source export.sh
   ```

2. **Board ESP32CAM**: ESP32CAM AI-Thinker

### Compilazione

1. **Naviga nella directory del progetto**:
   ```bash
   cd ESP32CAM_ESPIDF
   ```

2. **Configura il progetto**:
   ```bash
   idf.py set-target esp32
   idf.py menuconfig
   ```

3. **Compila il progetto**:
   ```bash
   idf.py build
   ```

4. **Carica il firmware**:
   ```bash
   idf.py -p /dev/cu.usbserial-110 flash
   ```

5. **Monitora i log**:
   ```bash
   idf.py monitor
   ```

## 📡 Configurazione WiFi

Modifica le credenziali WiFi nel file `main/main.c`:

```c
wifi_config_t wifi_config = {
    .sta = {
        .ssid = "IL_TUO_SSID",
        .password = "LA_TUA_PASSWORD",
    },
};
```

## 🌐 Interfaccia Web

Una volta avviato, il dispositivo sarà accessibile via web browser all'indirizzo IP assegnato dal router.

### Endpoint disponibili:

- **`/`** - Pagina principale con controlli
- **`/capture`** - Scatta una foto
- **`/photo`** - Visualizza l'ultima foto scattata
- **`/stream`** - Pagina streaming video
- **`/video`** - Stream MJPEG in tempo reale
- **`/status`** - Status JSON del sistema

## 🔄 Task FreeRTOS

Il progetto utilizza 3 task principali:

1. **LED Task** (Core 0, Priorità 1): Gestisce il lampeggiamento del LED
2. **Camera Task** (Core 1, Priorità 3): Gestisce le operazioni della fotocamera
3. **Webserver Task** (Core 0, Priorità 2): Gestisce il server HTTP

### Comunicazione tra Task

- **Event Groups**: Sincronizzazione WiFi
- **Queue**: Comunicazione tra task fotocamera
- **Mutex**: Accesso thread-safe alle risorse condivise

## 🎯 Funzionalità Avanzate

### Gestione Memoria
- Allocazione dinamica per le foto
- Liberazione automatica della memoria
- Gestione PSRAM per buffer fotocamera

### Ottimizzazioni Streaming
- Risoluzione QVGA (320x240) per streaming veloce
- Qualità JPEG ridotta (5) per latenza minima
- Buffer singolo per ridurre latenza
- FPS controllato (~30 FPS)

### Sicurezza Thread
- Mutex per accesso fotocamera
- Mutex per accesso webserver
- Timeout per operazioni critiche

## 📊 Monitoraggio e Debug

### Log Levels
- **ERROR**: Errori critici
- **WARN**: Avvisi
- **INFO**: Informazioni generali
- **DEBUG**: Debug dettagliato

### Metriche Disponibili
- FPS streaming
- Dimensione foto
- Tempo di acquisizione
- Status WiFi
- Uptime sistema

## 🔧 Personalizzazione

### Modifica Risoluzione
```c
camera_set_frame_size(FRAMESIZE_VGA);  // 640x480
camera_set_frame_size(FRAMESIZE_SVGA); // 800x600
```

### Modifica Qualità JPEG
```c
camera_set_jpeg_quality(10);  // Qualità migliore
camera_set_jpeg_quality(1);   // Qualità massima
```

### Modifica FPS Streaming
Modifica il delay nel file `webserver.c`:
```c
vTaskDelay(pdMS_TO_TICKS(30)); // ~30 FPS
```

## 🐛 Risoluzione Problemi

### Problemi Comuni

1. **Fotocamera non inizializza**:
   - Verifica connessioni hardware
   - Controlla configurazione pin
   - Verifica alimentazione

2. **WiFi non si connette**:
   - Verifica credenziali
   - Controlla segnale WiFi
   - Verifica configurazione router

3. **Streaming lento**:
   - Riduci qualità JPEG
   - Riduci risoluzione
   - Aumenta delay tra frame

4. **Memoria insufficiente**:
   - Libera foto precedenti
   - Riduci dimensione buffer
   - Verifica configurazione PSRAM

## 📈 Miglioramenti Futuri

- [ ] Integrazione YOLO per object detection
- [ ] Salvataggio foto su SD card
- [ ] Controllo remoto via MQTT
- [ ] Interfaccia web migliorata
- [ ] Supporto per più risoluzioni
- [ ] Configurazione via web
- [ ] Time-lapse automatico
- [ ] Motion detection

## 📄 Licenza

Questo progetto è rilasciato sotto licenza MIT.

## 🤝 Contributi

I contributi sono benvenuti! Per favore:

1. Fork il progetto
2. Crea un branch per la feature
3. Commit le modifiche
4. Push al branch
5. Apri una Pull Request

## 📞 Supporto

Per supporto o domande:
- Apri una issue su GitHub
- Contatta l'autore del progetto

---

**Nota**: Questo progetto è stato convertito dal framework Arduino IDE al framework ESP-IDF per sfruttare le funzionalità avanzate di FreeRTOS e una migliore gestione delle risorse. 