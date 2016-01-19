# laser-access

An access system for a laser cutter (or any other electric machine), based on Arduino, ESP8266 and NFC reader and member cards.
Easy update of tag database via Wifi link to a central access db. This is running just fine for a while here at Fablab Munich :)

* [./eagle](https://github.com/hierle/laser-access/eagle/)	&nbsp;		PCB (eagle) files for custom shield
* [./housing](https://github.com/hierle/laser-access/housing/)	&nbsp;		DXF drawing for laser cut housing
* [./laser_access](https://github.com/hierle/laser-access/laser_access/)	 	Arduino software

The software is running on an Arduino Mega 2560, packed with a "shield" (eagle files included),
holding breakout modules for RTC, NFC, Wifi (ESP8862-1), I2C-EEPROM and TFT touch display.

It's reading NFC (Mifare) cards nerby, comparing the serial to a database in internal eeprom,
granting access to known serials only. If tag serial is found in database, then
- card holder name and phone number (stored on the card) are displayed
- the 230V main relais of the laser cutter is turned on
If the dedicated "Update" card is seen, the database (of allowed card serial numbers), is downloaded from a Raspberry Pi Wifi access point.
Internal 4k eeprom of the Mega board holds up to 1000 users, external EEPROM holds up to 64kBytes log events.

Libraries used:

- https://github.com/adafruit/Adafruit_NFCShield_I2C
- https://github.com/adafruit/Adafruit-GFX-Library
- https://github.com/adafruit/Adafruit_ILI9341
- https://github.com/adafruit/Adafruit_STMPE610


Have fun :)
