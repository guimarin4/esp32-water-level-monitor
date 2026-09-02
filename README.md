# ESP32 Water Level Monitor

IoT-based water tank level monitoring system using an ESP32 DevKit V1 and a waterproof JSN-SR04T ultrasonic sensor.

The project aims to measure the water level of a tank, filter measurement noise, and provide real-time monitoring through a local web dashboard.

## Project Status

Current development stage:

* [x] ESP32 board testing
* [x] JSN-SR04T wiring
* [x] Basic ultrasonic sensor test
* [ ] Sensor reading validation
* [ ] Moving average filtering
* [ ] Water level percentage calculation
* [ ] Wi-Fi connectivity
* [ ] Local web server
* [ ] AJAX/JSON real-time dashboard
* [ ] Error handling
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
| Full tank distance  |  25 cm |
| Empty tank distance | 120 cm |
| Measurement range   |  95 cm |

## Pin Connections

| JSN-SR04T | ESP32                           |
| --------- | ------------------------------- |
| VCC       | 5V / VIN                        |
| GND       | GND                             |
| TRIG      | GPIO 5                          |
| ECHO      | GPIO 18 through voltage divider |

> Important: the JSN-SR04T ECHO output can operate at 5 V, while the ESP32 GPIO operates at 3.3 V. A voltage divider is therefore used between the ECHO pin and GPIO 18.

## Voltage Divider

```text
JSN-SR04T ECHO
      |
     1 kΩ
      |
      +------ GPIO 18
      |
     2 kΩ
      |
     GND
```

## Planned Architecture

```text
JSN-SR04T
    |
    v
ESP32
    |
    +---- Sensor filtering
    |
    +---- Water level calculation
    |
    +---- Wi-Fi
             |
             v
        Local Web Server
             |
             v
        AJAX / JSON Dashboard
```

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
