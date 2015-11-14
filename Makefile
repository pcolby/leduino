ARDUINO = ~/opt/arduino-1.6.6/arduino
PROJECT = leduino
SOURCE  = ${PROJECT}.ino

check:	${SOURCE}
	$(ARDUINO) --verify $<

install: ${SOURCE}
	$(ARDUINO) --upload $<

.PHONY: check install
