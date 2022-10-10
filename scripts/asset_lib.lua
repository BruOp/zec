ASSET_LIB_DIR = (path.getabsolute("..") .. "/asset_lib/")

project("asset_lib")
uuid(os.uuid("asset_lib"))
kind "SharedLib"

files {
  path.join(ASSET_LIB_DIR, name, "**.cpp"),
  path.join(ASSET_LIB_DIR, name, "**.h"),
}

debugdir(RUNTIME_DIR)

includedirs {
  EXTERNAL_DIR,
  path.join(ZEC_DIR, "src"),
}

flags {
  "FatalWarnings"
}

defines {
  "_SECURE_SCL=0",
}

links {
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
