setup: fix

fix: generate
	sed -i 's/build\\\native\\\WinPixEvent/build\\\WinPixEvent/g' ./.build/**.vcxproj

generate:
	./tools/premake5.exe --file=scripts/premake.lua vs2019
