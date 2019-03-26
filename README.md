# ESP32-PICO-D4_MQTT-LED-Board

## Plan

 Create a portable board that:

- Powers 1-4 LEDS (From a single LED channel to RGBW)
- Communicates via MQTT
- Supports using a momentary pushbutton and rotary encored for on/off/brightness
- Has a usb-chargable battery for portability



## Features already implemented

- Power a single LED
- Subscribe to an MQTT topic
- Respond to `ON`/`OFF`/'TOGGLE` and `brightness` commands
- Integration with Homeassistant
