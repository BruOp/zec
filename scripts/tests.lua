
---------------- Testing
project ("zec_tests")
  uuid(os.uuid("zec-tests"))

  kind "ConsoleApp"

  files {
    path.join(TESTS_DIR, "**.cpp"),
  }

  includedirs {
    path.join(ZEC_DIR, "src"),
    EXTERNAL_DIR,
  }

  debugdir(RUNTIME_DIR)

  flags {
    "FatalWarnings"
  }

  defines {
    "_SECURE_SCL=0"
  }

  links {
    "zec_lib"
  }

  configuration {"vs*", "x64"}
  linkoptions {
    "/ignore:4199" -- LNK4199: /DELAYLOAD:*.dll ignored; no imports found from *.dll
  }

  configuration{}
