## Useful information about the project


### Requirements

- ESP-IDF v6.0 or newer
- ESP32-S3 board
- OV3660 camera sensor
- Python 3.8+ (for scripts and notebooks)

### Compiling and Flashing

#### Install ESP-IDF (outside this repo)
```bash
# Clone esp-idf repository
git clone https://github.com/espressif/esp-idf.git
cd esp-idf

# Install esp-idf for your specific architecture
./install.sh esp32s3

# Enable the environment (you can now use idf.py commands on this terminal)
source export.sh 


# To always have the environment enabled, add this line to your .bashrc or .zshrc
source $HOME/esp/esp-idf/export.sh
#This way, export.sh is activated every time the terminal is opened.
#NOTE: please verify that the path $HOME/esp/esp-idf/ is correct for your specific installation.

```


#### Build commands (inside this repo)
```bash
# Set the specific esp32 target
idf.py set-target esp32s3

# Clean the build
idf.py fullclean

# Install all the dependencies
idf.py reconfigure

# Compile
idf.py build

# Compile + flash to the device
idf.py -p /dev/tty.usbmodemXXXX flash

# Serial monitor
idf.py -p /dev/tty.usbmodemXXXX monitor

# Change this to be able to enter keyboard inputs on the serial monitor
idf.py menuconfig -> Component config → ESP System Settings → Channel for console output -> USB CDC

# Enable external psram (for model activations and high resolution images)
idf.py menuconfig -> enable PSIRAM/PSRAM #often, only dram is enabled by default

# If the flash memory is not enough, you can increase it from the configuration menu (in the ESP32-S3, 4 MB of the 16 MB are used by default).
idf.py menuconfig -> Component config -> increase flash memory

# To increase cpu clock frequency
idf.py menuconfig -> Component config -> set 240 mhz as clock frequency

#If you encounter problems flashing the code or opening the serial monitor...
-> On the physical board, hold down the BOOT button, press and release the RST button, and then release the initial BOOT button.
-> This will enter download mode and allow you to upload the firmware. Press RST again to exit download mode. You can now open the serial monitor.
```

### WiFi Configuration
Modify the file `main.cpp` to set the WiFi credentials for the web server:
```cpp
#define WIFI_SSID "Your_WiFi_ssid"
#define WIFI_PASS "Your_Password"
```
The specific IP address needed to connect to the web server from your browser will be displayed in the serial monitor.

### Available endpoints for web interface
- `GET /` - Web server homepage
- `GET /capture` - Take a photo
- `GET /photo` - View the last photo taken
- `POST /inference` - Run model inference

