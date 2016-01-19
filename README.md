# laser-access

Access system for a laser cutter (or any other electric machine), based on Arduino, ESP8266 and NFC member cards.
Easy update of access data via Wifi link to central access db. Running just fine here at Fablab Munich :)
Basically the system is restricting access to a laser cutter to valid members only.

[./eagle](https://github.com/hierle/laser-access/eagle/)	|		PCB (eagle) files for custom shield
[./housing](https://github.com/hierle/laser-access/housing/)	|		DXF drawing for laser cut housing
[./laser_access](https://github.com/hierle/laser-access/laser_access/)	| 	Arduino software

This software is running on an Arduino Mega 2560, packed with a "shield" (eagle files included),
holding breakout modules for RTC, NFC, Wifi (ESP8862-1), I2C-EEPROM and TFT touch display.

It's reading NFC (Mifare) cards, comparing the serial to a database in internal eeprom,
granting access to known serials only. If serial is found, card holder name and phone number (stored on the card) are displayed,
then switching the relais to turn on the machine (here : laser cutter).
The database containing the valid members (card serial numbers), is download from a Raspberry Pi Wifi access point,
once a dedicated "Update" card is seen.

Internal 4k eeprom of the Mega board holds up to 1000 users, external EEPROM holds up to 64kBytes log events.

Libraries used:

- https://github.com/adafruit/Adafruit_NFCShield_I2C
- https://github.com/adafruit/Adafruit-GFX-Library
- https://github.com/adafruit/Adafruit_ILI9341
- https://github.com/adafruit/Adafruit_STMPE610


Have fun :)
