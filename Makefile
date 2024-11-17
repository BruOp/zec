RUNTIME_DIR = ./runtime
BUILD_DIR = ./.build
DEBUG_BIN_DIR = $(BUILD_DIR)/bin/Debug

setup: generate

generate:
	./tools/premake5.exe --file=scripts/premake.lua vs2022

flight_helmet:
	cd $(RUNTIME_DIR) && ../$(DEBUG_BIN_DIR)/asset_converter/asset_converter.exe ../../glTF-Sample-Models/2.0/FlightHelmet/glTF/FlightHelmet.gltf ./models/FlightHelmet/FlightHelmet.azec

sponza:
	cd $(RUNTIME_DIR) && ../$(DEBUG_BIN_DIR)/asset_converter/asset_converter.exe ../../glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf ./models/Sponza/Sponza.azec