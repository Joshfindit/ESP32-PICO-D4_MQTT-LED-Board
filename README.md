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
- Respond to `ON`/`OFF`/'TOGGLE` and `brightness` commands
- Integration with Homeassistant
