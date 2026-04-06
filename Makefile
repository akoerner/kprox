SELL:=/bin/bash

.DEFAULT_GOAL := help

ROOT_DIR:=$(shell dirname "$(realpath $(firstword $(MAKEFILE_LIST)))")

.EXPORT_ALL_VARIABLES:

HOST            ?= kprox.local
KPROX_API_KEY   ?= kprox1337
BIN_PATH              := .pio/build/m5stack-atoms3/firmware.bin
SPIFFS_BIN_PATH       := .pio/build/m5stack-atoms3/spiffs.bin
CARDPUTER_BIN_PATH    := .pio/build/m5stack-cardputer/firmware.bin
CARDPUTER_SPIFFS_PATH := .pio/build/m5stack-cardputer/spiffs.bin
TOOLS_DIR       := $(ROOT_DIR)/tools

GREEN  := $(shell printf '\033[0;32m')
YELLOW := $(shell printf '\033[0;33m')
NC     := $(shell printf '\033[0m')

.PHONY: _prep_web
_prep_web:
	@echo "${YELLOW}Preparing web assets...${NC}"
	npm install html-minifier clean-css terser fs-extra mkdirp
	node build.js

## Help: Show this menu
.PHONY: help
help:
	@echo "${GREEN}Available Commands:${NC}"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-20s\033[0m %s\n", $$1, $$2}'

## build: Install dependencies, build assets, and flash via USB (firmware + filesystem)
.PHONY: build
build: _prep_web ## Full USB build and flash (firmware + filesystem). ESP32 must be in bootloader mode.
	pio run -t uploadfs
	platformio run -t upload
	make filesystem_check

## build-ota: Compile firmware and SPIFFS binaries for OTA update
.PHONY: build-ota
build-ota: _prep_web ## Build firmware and filesystem binaries for OTA
	platformio run
	platformio run -t buildfs
	@echo "${GREEN}Firmware: $(BIN_PATH)${NC}"
	@echo "${GREEN}Filesystem: $(SPIFFS_BIN_PATH)${NC}"

## build_cardputer: Build SPIFFS and firmware for Cardputer then flash both via USB
.PHONY: build_cardputer
build_cardputer: _prep_web ## Build and flash firmware + filesystem for Cardputer (USB). Device must be in bootloader mode.
	@echo "${YELLOW}Cleaning Cardputer build (ensures library flags apply)...${NC}"
	pio run -e m5stack-cardputer -t clean
	@echo "${YELLOW}Building Cardputer filesystem...${NC}"
	pio run -e m5stack-cardputer -t buildfs
	@echo "${YELLOW}Building Cardputer firmware...${NC}"
	pio run -e m5stack-cardputer
	@echo "${YELLOW}Flashing filesystem...${NC}"
	pio run -e m5stack-cardputer -t uploadfs
	@echo "${YELLOW}Flashing firmware...${NC}"
	pio run -e m5stack-cardputer -t upload
	@echo "${GREEN}Cardputer flash complete.${NC}"
	@echo "${GREEN}Firmware:    $(CARDPUTER_BIN_PATH)${NC}"
	@echo "${GREEN}Filesystem:  $(CARDPUTER_SPIFFS_PATH)${NC}"

# Upload a binary via authenticated multipart POST.
# Usage: $(call _kprox_ota_upload,api/ota,firmware,path/to/file.bin)
define _kprox_ota_upload
	@bash -c ' \
		source $(TOOLS_DIR)/kprox_crypto.sh; \
		nonce=$$(_kprox_get_nonce); \
		hmac=$$(_kprox_hmac "$$nonce"); \
		curl -X POST http://$(HOST)/$(1) \
			-F "$(2)=@$(3)" \
			-H "X-Auth: $$hmac" \
			-H "Accept: application/json"; \
	'
endef

## ota: Upload firmware only via HTTP API (authenticated)
.PHONY: ota-firmware
ota-firmware: build-ota ## Upload firmware binary via OTA
	@echo "${YELLOW}Uploading firmware to http://$(HOST)/api/ota ...${NC}"
	$(call _kprox_ota_upload,api/ota,firmware,$(BIN_PATH))
	@echo "${GREEN}Firmware OTA complete.${NC}"
	make filesystem_check

## ota-spiffs: Upload SPIFFS filesystem only via HTTP API (authenticated)
.PHONY: ota-spiffs
ota-spiffs: ## Build and upload SPIFFS filesystem via OTA (does not reflash firmware)
	@echo "${YELLOW}Building filesystem image...${NC}"
	platformio run -t buildfs
	@echo "${YELLOW}Uploading filesystem to http://$(HOST)/api/ota/spiffs ...${NC}"
	$(call _kprox_ota_upload,api/ota/spiffs,spiffs,$(SPIFFS_BIN_PATH))
	@echo "${GREEN}Filesystem OTA complete.${NC}"

## ota-full: Upload firmware and SPIFFS filesystem via OTA (authenticated)
.PHONY: ota
ota: build-ota ## Build and upload firmware + filesystem via OTA
	@echo "${YELLOW}Uploading firmware to http://$(HOST)/api/ota ...${NC}"
	$(call _kprox_ota_upload,api/ota,firmware,$(BIN_PATH))
	@echo
	@echo "${GREEN}Firmware OTA complete. Waiting for reboot...${NC}"
	@sleep 20
	@echo "${YELLOW}Uploading filesystem to http://$(HOST)/api/ota/spiffs ...${NC}"
	$(call _kprox_ota_upload,api/ota/spiffs,spiffs,$(SPIFFS_BIN_PATH))
	@echo
	@echo "${GREEN}Full OTA complete.${NC}"
	make filesystem_check

ESPTOOL := $(HOME)/.platformio/packages/tool-esptoolpy/esptool.py

## wipe: Completely erase internal flash (NVS, firmware, SPIFFS, OTA data)
.PHONY: wipe
wipe: ## Erase entire internal flash — device will need a full reflash afterwards
	@echo "${YELLOW}Erasing entire internal flash...${NC}"
	python3 $(ESPTOOL) --chip esp32s3 erase_flash
	@echo "${GREEN}Flash erased. Run 'make build' to reflash.${NC}"


clean: ## Remove data, node_modules, and .pio folders
	rm -rf data node_modules .pio package-lock.json *.bin
	cd android && make clean

## monitor: Open the PlatformIO serial monitor
.PHONY: monitor
monitor: ## Open serial device monitor
	pio device monitor

## mDNS: Run the mDNS discovery tool
.PHONY: mDNS
mDNS: ## Run discovery script
	python3 tools/mDNS.py

.PHONY: filesystem_check
filesystem_check: ## Print filesystem usage stats
	@echo "${GREEN}--- Filesystem Usage Report ---${NC}"
	@$(eval APP_BYTES=$(shell size .pio/build/m5stack-atoms3/firmware.elf | awk 'NR==2 {print $$1 + $$2}'))
	@$(eval APP_MB=$(shell echo "scale=2; $(APP_BYTES) / 1048576" | bc))
	@echo "${YELLOW}Application (Code):${NC}"
	@echo "  Used:   $(APP_MB) MB"
	@echo "  Limit:  3.00 MB"
	@echo "  Free:   $(shell echo "scale=2; 3.00 - $(APP_MB)" | bc) MB"
	@echo ""
	@$(eval DATA_BYTES=$(shell du -sb data/ | cut -f1))
	@$(eval DATA_MB=$(shell echo "scale=2; $(DATA_BYTES) / 1048576" | bc))
	@echo "${YELLOW}Filesystem (Data Folder):${NC}"
	@echo "  Files:  $(DATA_MB) MB"
	@echo "  Limit:  1.94 MB"
	@echo "  Status: $(shell [ $$(echo "$(DATA_MB) > 1.94" | bc) -eq 1 ] && echo "${RED}OVER LIMIT${NC}" || echo "${GREEN}OK${NC}")"
	@echo "${GREEN}-----------------------------${NC}"
