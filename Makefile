RUNTIME_DIR = ./runtime
BUILD_DIR = ./.build
DEBUG_BIN_DIR = $(BUILD_DIR)/bin/Debug
FLIGHT_HELMET_INPUT_DIR = ../glTF-Sample-Models/2.0/FlightHelmet/glTF
FLIGHT_HELMET_OUTPUT_DIR = ./models/FlightHelmet
setup: generate

generate:
	./tools/premake5.exe --file=scripts/premake.lua vs2022

flight_helmet:
	cd $(RUNTIME_DIR) && \
	mkdir -p $(FLIGHT_HELMET_OUTPUT_DIR) && \
	../$(DEBUG_BIN_DIR)/asset_converter/asset_converter.exe ../$(FLIGHT_HELMET_INPUT_DIR)/FlightHelmet.gltf $(FLIGHT_HELMET_OUTPUT_DIR)/FlightHelmet.azec

sponza:
	cd $(RUNTIME_DIR) && ../$(DEBUG_BIN_DIR)/asset_converter/asset_converter.exe ../../glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf ./models/Sponza/Sponza.azec