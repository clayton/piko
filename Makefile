-include Makefile.local

PORT ?= /dev/cu.usbmodem1101
FQBN = esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,UploadSpeed=460800
CONFIG_TEMPLATE ?= radar/config.h.example
CONFIG_DEST ?= radar/config.h

.PHONY: setup secrets build flash geiger-build geiger-flash raises-build raises-flash piko-build piko-flash test monitor restore clean-secrets

setup:
	arduino-cli core install esp32:esp32@3.3.5
	arduino-cli lib install 'GFX Library for Arduino@1.6.4' 'ArduinoJson@7.4.2' 'SensorLib@0.3.3' 'XPowersLib@0.3.3'

secrets:
	cp '$(CONFIG_TEMPLATE)' '$(CONFIG_DEST)'
	chmod 600 '$(CONFIG_DEST)'

build:
	rm -rf build
	arduino-cli compile --fqbn '$(FQBN)' --libraries libraries --output-dir build radar

flash: build
	arduino-cli upload -p $(PORT) --fqbn '$(FQBN)' --input-dir build radar

geiger-build:
	rm -rf build-geiger
	arduino-cli compile --fqbn '$(FQBN)' --libraries libraries --output-dir build-geiger internet_geiger

geiger-flash: geiger-build
	arduino-cli upload -p $(PORT) --fqbn '$(FQBN)' --input-dir build-geiger internet_geiger

raises-build:
	rm -rf build-raises
	arduino-cli compile --fqbn '$(FQBN)' --libraries libraries --output-dir build-raises raises_piko

raises-flash: raises-build
	arduino-cli upload -p $(PORT) --fqbn '$(FQBN)' --input-dir build-raises raises_piko

piko-build:
	rm -rf build-piko
	arduino-cli compile --fqbn '$(FQBN)' --libraries libraries --output-dir build-piko piko

piko-flash: piko-build
	arduino-cli upload -p $(PORT) --fqbn '$(FQBN)' --input-dir build-piko piko

test:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic tests/button_logic_test.cpp -o /tmp/piko-button-test
	$(CXX) -std=c++17 -Wall -Wextra -pedantic tests/power_logic_test.cpp -o /tmp/piko-power-test
	/tmp/piko-button-test
	/tmp/piko-power-test

monitor:
	arduino-cli monitor -p $(PORT) --config baudrate=115200

restore:
	@test -n "$(FACTORY_BACKUP)" || { echo 'Set FACTORY_BACKUP in Makefile.local'; exit 1; }
	uvx --from esptool esptool --port $(PORT) --baud 460800 write-flash 0 '$(FACTORY_BACKUP)'

clean-secrets:
	rm -f radar/config.h radar/secrets.h raises_piko/config.h piko/config.h
