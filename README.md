# Arduino-based TIC display

## Overview

This repository contains a energy meter decoder and display project for french meters using an Arduino Uno board together with an [Adafruit I2C LCD shield for Arduino](https://www.adafruit.com/product/772).

French energy meters (like the Linky meter) continuously output metering information on a user-accessible connector on the meter (on most meters, two pins (I1, I2) are available for this).
This data is called "TIC" and this project aims to decode that data and display real-time power consumption on the Arduino and its LCD display shield.

In order to convert the TIC data signal from the meter into a standard TTL-level serial stream, I soldered a home-made serial adapter including optocoupler and transitor-based level shifter. Examples can be found here: https://faire-ca-soi-meme.fr/domotique/2016/09/12/module-teleinformation-tic/

Once the serial adapter is connected to the Arduino's GND, Vcc and Serial RX (PIN on Arduino Uno), the Arduino will be able to receive and decode TIC stream data sent by the meter.

The software is currently configured to decode Linky data in standard TIC mode (9600 bauds), which is not the default built-in mode for Linky meters.
In order to switch to this more verbose mode (that also provides more data), you need to make a request to your energy vendor. As an alternative, the code can be slightly tweaked to switch to historical mode and to work at 1200 bauds (this is actually the default mode for Linky meters).

## User Guide

### Setup & installation

In order to build and program this project into an Arduino board, you will need a working Arduino IDE.
You will also have to import the following external library that this code depends on (in order to drive the LCD display):
* AdaFruit RGB LCD shield library that can be installed directly from the built-in Arduino IDE's library manager

To compile, first clone the content of this repository and the TIC library submodule:
```
git clone https://github.com/lains/linky_remote_display_arduino.git
git submodule init stm32-linky-display
```

Once these prerequisites are satisfied, compile with the Arduino IDE.
Still in the Arduino IDE, program the Arduino Uno target that you prepared (with the Adafruit I2C LCD shield mounted). Once programming is finished, you should see a message showing "Syncing..." displayed while the TIC signal is being waited for.
The current withdrawn power will then appear shortly after you connect the wires to the power meter.

> **Warning**  
> If "No signal!" appears after a few seconds, no TIC signal could be decoded from the serial port.
