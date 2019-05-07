# ESP32-PICO-D4_MQTT-LED-Board

## Current hardware

![](https://ae01.alicdn.com/kf/HTB1WFm1e3fH8KJjy1zcq6ATzpXaL.jpg?size=102586&height=1000&width=1000&hash=1f1c62b0595c355da2b51ddf27a1f4d3)
[TTGO T7 (Aliexpress link)](https://www.aliexpress.com/item/TTGO-T7-ESP32-WiFi-Module-ESP-32-Bluetooth-PICO-D4-4MB-SPI-Flash-ESP-32-development/32841601867.html?spm=a2g0s.9042311.0.0.1a934c4dHxDQme)

## Plan

 Create a portable board that:

- Powers 1-4 LEDS (From a single LED channel to RGBW)
- Communicates via MQTT
- Supports using a momentary pushbutton and rotary encored for on/off/brightness
- Has a usb-chargable battery for portability



## Features already implemented

- Power a single LED
- Use the HIGHPWM counters built in to the Pico D4 Mini (can go up to 300,000kHz at 10-bit (`0-1023`))
- Subscribe to an MQTT topic
- Respond to `ON`/`OFF`/`TOGGLE` and `brightness` commands
- Integration with Homeassistant
- *Bonus: on/off/brightness changes are smoothly faded. Currently at 300ms


## Development on Ubuntu

* Install ESP-IDF.

* Install Eclipse Paho for ESP32:
```bash
cd /path/to/esp/esp-idf/components
git clone --recursive https://github.com/nkolban/esp32-snippets/tree/master/networking/mqtt/paho_mqtt_embedded_c
mv esp32-snippets/networking/mqtt/paho_mqtt_embedded_c paho
rm -r esp32-snippets
```

*In addition to the documentation on the ESP32, these steps were neccessary to set up Ubuntu server:*

```bash
cd ~/
wget https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-80-g6c4433a-5.2.0.tar.gz
mkdir -p ~/esp
cd ~/esp
tar -xzf ~/xtensa-esp32-elf-linux64-1.22.0-80-g6c4433a-5.2.0.tar.gz
git clone --recursive https://github.com/espressif/esp-idf.git
cd ~/esp/esp-idf/
git clone --recursive https://github.com/nkolban/esp32-snippets/tree/master/networking/mqtt/paho_mqtt_embedded_c
mv esp32-snippets/networking/mqtt/paho_mqtt_embedded_c paho
rm -r esp32-snippets
sudo apt install python-pip
export IDF_PATH=~/esp/esp-idf ## Note: has to be present in the environment for compiling and flashing
python -m pip install --user -r $IDF_PATH/requirements.txt
apt install libncurses5-dev flex bison gperf
sudo usermod -a -G tty $(whoami) ## Possibly not needed
sudo usermod -a -G dialout $(whoami)
```

* Configure, compile and flash the board

```bash
make menuconfig # Pay special attention to the `MQTT_LED` section
make flash
```
*Note: you can also `make monitor` for the debug readout from the USB-connected board*
