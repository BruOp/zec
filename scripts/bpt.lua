BPT_DIR = (ZEC_DIR .. "bpt/")

project("example-" .. name)
uuid(os.uuid("example-" .. name))
kind "WindowedApp"

files {
  path.join(BPT_DIR, name, "**.cpp"),
  path.join(BPT_DIR, name, "**.h")
}

removefiles {
  path.join(BPT_DIR, name, "**.bin.h")
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
  -- "_ITERATOR_DEBUG_LEVEL=0"
}

links {
  "zec_lib"
}

configuration {"vs*", "x64"}
linkoptions {
  "/ignore:4199" -- LNK4199: /DELAYLOAD:*.dll ignored; no imports found from *.dll
}

configuration{}
