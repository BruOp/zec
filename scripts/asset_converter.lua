ASSET_CONVERTER_SRC_DIR = (path.getabsolute("..") .. "/asset_converter/")

project("asset_converter")
uuid(os.uuid("asset_converter"))
kind "ConsoleApp"

files {
  path.join(ASSET_CONVERTER_SRC_DIR, "**.cpp"),
  path.join(ASSET_CONVERTER_SRC_DIR, "**.h")
}

debugdir(RUNTIME_DIR)

includedirs {
  EXTERNAL_DIR,
  ASSET_LIB_DIR,
  ZEC_SRC_DIR,
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

filter { "configurations:Release" }
  defines {
    "_ITERATOR_DEBUG_LEVEL=0"
  }
filter{}
