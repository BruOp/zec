ASSET_CONVERTER_SRC_DIR = (path.getabsolute("..") .. "/asset_converter/")

project("asset_converter")
uuid(os.uuid("asset_converter"))
kind "ConsoleApp"

files {
  path.join(ASSET_CONVERTER_SRC_DIR, name, "**.cpp"),
  path.join(ASSET_CONVERTER_SRC_DIR, name, "**.h")
}

debugdir(RUNTIME_DIR)

includedirs {
  EXTERNAL_DIR,
  path.join(ZEC_DIR, "src"),
  path.join(ZEC_DIR, "asset_lib")
}

flags {
  "FatalWarnings"
}

defines {
  "_SECURE_SCL=0",
}

links {
  "asset_lib",
  "zec_lib"
}

DLL_PATH = path.join(EXTERNAL_DIR, "../bin/*.dll")
postbuildcommands { "cp %{DLL_PATH} %{cfg.targetdir}" }

configuration {"vs*", "x64"}
linkoptions {
  "/ignore:4199" -- LNK4199: /DELAYLOAD:*.dll ignored; no imports found from *.dll
}

configuration { "Release" }
  defines {
    "_ITERATOR_DEBUG_LEVEL=0"
  }

configuration{}
