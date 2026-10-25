# ESP32 Water Level Monitor

IoT-based water tank level monitoring system using an ESP32 DevKit V1 and a waterproof JSN-SR04T ultrasonic sensor.

The project aims to measure the water level of a tank, filter measurement noise, and provide real-time monitoring through a local web dashboard.

## Project Status

Current development stage:

* [x] ESP32 board testing
* [x] JSN-SR04T wiring
* [x] Basic ultrasonic sensor test
* [ ] Sensor reading validation
* [x] Median filtering of valid samples
* [x] Water level percentage calculation
* [x] Wi-Fi connectivity
* [x] Local web server
* [x] AJAX/JSON real-time dashboard
* [x] Invalid readings and missing echoes handled
* [ ] Optional MQTT integration
* [ ] Optional LoRa communication

## Hardware

* ESP32 DevKit V1
* JSN-SR04T waterproof ultrasonic sensor
* 1 kΩ resistor
* 2 kΩ resistor
* Jumper wires
* USB power supply
* Water tank

## Tank Parameters

| Parameter           |  Value |
| ------------------- | -----: |
| Full tank distance  |  15 cm |
| Empty tank distance |  45 cm |
| Water height        |  30 cm |

These are the current firmware calibration values. The provisional minimum sensor
distance is 25 cm; confirm it against the exact module's datasheet. With the
current 15 cm full-tank distance, the upper part of the water level is outside
the accepted range (the highest accepted level is about 66.7%). Reposition the
sensor with clearance above its minimum range at full tank, then update
`DISTANCIA_CHEIA` to the actual distance. Keep `ALTURA_AGUA` equal to the actual
usable water height. The dashboard and serial output warn about this mismatch.

## Measurement Behavior

* During testing, a group of five samples is collected every two seconds, with
  at least 70 ms between triggers. Email sending is disabled.
* Samples outside `DISTANCIA_MIN_SENSOR` through `DISTANCIA_VAZIA + 5 cm`,
  including missing echoes, are discarded. At least three valid samples are
  required; their median determines distance, height, and level.
* An invalid group preserves the last valid values and their original timestamp.
  The dashboard marks them as outdated. Before the first valid reading, it shows
  placeholders. Invalid readings never imply that the tank is full.
* Wi-Fi startup waits at most ten seconds. Measurements continue through the
  serial port when offline, and Wi-Fi reconnection is attempted every 30 seconds.

The filter reduces isolated spikes; persistent incorrect echoes inside the
accepted range can still affect measurements. Validate the installation at both
full and empty tank, with a clear path from the sensor face to the water.

## Verification

Build the firmware with `pio run`. The filter and dashboard tests can also run
without the ESP32. With a native C++ compiler and Node.js installed, run from the
project directory in PowerShell:

```powershell
g++ -std=c++11 -Wall -Wextra -Werror -I include test/test_measurement.cpp -o "$env:TEMP\water-monitor-filter-test.exe"
& "$env:TEMP\water-monitor-filter-test.exe"
node test/test_dashboard.js
```

## Pin Connections

| JSN-SR04T | ESP32                           |
| --------- | ------------------------------- |
| VCC       | 5V / VIN                        |
| GND       | GND                             |
| TRIG      | GPIO 5                          |
| ECHO      | GPIO 18 through voltage divider |

> Important: the JSN-SR04T ECHO output can operate at 5 V, while the ESP32 GPIO operates at 3.3 V. A voltage divider is therefore used between the ECHO pin and GPIO 18.

## Future Improvements

Possible future improvements include:

* MQTT integration
* Home Assistant integration
* Low-level alerts
* Historical data logging
* LoRa communication for remote installations
* OLED local display
* Battery or solar-powered operation

## License

This project is released under the MIT License.
