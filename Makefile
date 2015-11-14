ARDUINO = ~/opt/arduino-1.6.6/arduino

default: leduino.foo

%.foo: %.ino
	$(ARDUINO) --verify $<

.PHONY: install
install: leduino.ino
	$(ARDUINO) --upload $<
