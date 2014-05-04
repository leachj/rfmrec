rfmrec
======

This is a tool to receive temperature, light and voltage readings from a [TinyTX](http://nathan.chantrell.net/tinytx-wireless-sensor/) based sensor using the [rfm12b RF module](http://www.hoperf.com/upload/rf/rfm12b.pdf). In order to compile this you will need the [rfm12b-linux](https://github.com/gkaindl/rfm12b-linux) module. Update the entry in the makefile to point to this.

Parameters such as the group ID, jeenode ID and emoncms API key can be set at the top of the rfmrec.c file

The multisensor folder contains an arduino sketch that works in conjuntion with the library and includes a temperature sensor, lgiht sensor, PIR sensor and measurements of the sensor battery voltage. In order for it ti work you will need to use the modified version of the dallas temperature library and the original is just too big for the ATTiny84. This version of the library assumes parasitic power so i was able to strip out some of the code.
