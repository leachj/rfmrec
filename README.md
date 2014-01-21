rfmrec
======

This is a tool to receive temperature, light and voltage readings from a [TinyTX](http://nathan.chantrell.net/tinytx-wireless-sensor/) based sensor using the [rfm12b RF module](http://www.hoperf.com/upload/rf/rfm12b.pdf). In order to compile this you will need the [rfm12b-linux](https://github.com/gkaindl/rfm12b-linux) module. Update the entry in the makefile to point to this.

Parameters such as the group ID, jeenode ID and emoncms API key can be set at the top of the rfmrec.c file
