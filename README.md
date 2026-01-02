# WifiSensors

## Overview

**WifiSensors** is a project to work with multiple devices connected to Arduino and manage them over wifi.
Main idea about this code is to have easy communication with automation systems over simple API.

### Supported devices

  - BUTTON, SWITCH (DIGITAL INPUT)
  - GENERIC ANALOG INPUT (ANALOG INPUT)
  - GENERIC DIGITAL INPUT (DIGITAL INPUT)
  - MOTION (DIGITAL INPUT)
  - RELAY (OUTPUT)
  - DHT22 (INPUT)
  - ONEWIRE DALLAS TEMP SENSOR (INPUT)

## Run

### Startup

Initially starts in AP mode with SSID prefix ARDUINO_ and serves on 10.10.10.1.
After providing SSID and passwd it switch to wifi Client mode.
In case of connection problem it switch back to AP mode after 30 sec.

### Reset

To clear all stored data connect pin 13 with GND and click on reset button (disconnect GND after 3 seconds).

## API Reference

NOTE: API could change any time in version < 1.0.0

| method | path | Description | payload |
|----------|------------|------------|------------|
| GET | / | devices values (in AP config mode html form to porvide credentials) |  |
| GET | /devices | list current devices configuration and values |  |
| GET | /devicestypes | list supported devices types |  |
| GET | /pinout | pins configuration |  |
| GET | /pinsvalues | raw pins values |  |
| GET | /status | device status |  |
| POST | /config?id=[device id] | update config on device | configname=[configValue]&callback=[urlencode(http://hostname:port/path?arg1=value1)]&auth_header=[Basic+XXX]&interval=[interval millis] |
| POST | /creds | handle values from config html form (in AP config mode) |  |
| POST | /device?type=[device type see /devicestypes]&pin0=[pinId A.. or D..]&pin0type=[INPUT|OUTPUT|INPUT_PULLUP]&interval=[update interval millis] | add device | see payload for /config |
| POST | /pinout?id=[pinId A.. or D..] | configure pin |  |
| POST | /set?id=[pinId A.. or D..] | set digital pin if not used by any device |  |
| POST | /turnon?id=[device id] | button/realay on |  |
| POST | /turnoff?id=[device id] | button/realay off |  |
| POST | /unset?id=[pinId A.. or D..] | unset digital pin if not used by any device |  |
| DELETE | /device?id=[device id] | set configured device as not acive (SOFT DELETE) |  |

## License

This project is licensed under the **MIT License**. Feel free to use it and modify it on your own fork.

## Contributing

Pull requests with fixes are welcome! New functionalities and extensions will be not discussed in this repository.
