ASSET_LIB_DIR = (path.getabsolute("..") .. "/asset_lib/")

project("asset_lib")
uuid(os.uuid("asset_lib"))
kind "StaticLib"

files {
  path.join(ASSET_LIB_DIR, "**.cpp"),
  path.join(ASSET_LIB_DIR, "**.h"),
}

debugdir(RUNTIME_DIR)

includedirs {
  EXTERNAL_DIR,
  ZEC_SRC_DIR,
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

filter { "configurations:Release" }
  defines {
    "_ITERATOR_DEBUG_LEVEL=0"
  }
filter{}
