ZEC_DIR = (path.getabsolute("..") .. "/")
TESTS_DIR = (path.getabsolute("..") .. "/tests/")
RUNTIME_DIR = (path.getabsolute("..") .. "/runtime/")
EXAMPLES_DIR = (ZEC_DIR .. "examples/")
EXTERNAL_DIR = (ZEC_DIR .. "external/include/")
EXTERNAL_LIB_DIR  = (ZEC_DIR .. "external/lib/")

local BUILD_DIR = (ZEC_DIR .. ".build/")

include("./examples.lua")
--
-- Solution
--
workspace "zec"
  language "C++"
  configurations {"Debug", "Release"}
  platforms {"x64"}
  startproject "zec_lib"
  cppdialect "C++latest"
  premake.vstudio.toolset = "v142"
  location (BUILD_DIR)

  filter { "configurations:Debug" }
  symbols "On"
  filter { "configurations:Release" }
  optimize "On"
  -- Reset the filter for other settings
  filter { }

  -- nuget {
  --   "directxtk12_desktop_2017:2020.8.15.1"
  -- }

  libdirs { EXTERNAL_LIB_DIR }
  targetdir (BUILD_DIR .. "bin/%{prj.name}/%{cfg.longname}")
  objdir (BUILD_DIR .. "obj/%{prj.name}/%{cfg.longname}")

  floatingpoint "fast"

  defines {
    "WIN32",
    "_WIN32",
    "_WIN64",
    "_SCL_SECURE=0",
    "_SECURE_SCL=0",
    "_SCL_SECURE_NO_WARNINGS",
    "_CRT_SECURE_NO_WARNINGS",
    "_CRT_SECURE_NO_DEPRECATE",
    "USE_D3D_RENDERER"
  }
  linkoptions {
    "/ignore:4221", -- LNK4221: This object file does not define any previously undefined public symbols, so it will not be used by any link operation that consumes this library
    "/ignore:4006", -- LNK4006
  }

  disablewarnings {
    "4267",
    "28612",
  }

---
--- Projects
---

ZEC_SRC_DIR = path.join(ZEC_DIR, "src")
project("zec_lib")
  uuid(os.uuid("zec_lib"))
  kind "StaticLib"

  files {
    path.join(ZEC_SRC_DIR, "shaders/**.hlsl"),
    path.join(ZEC_SRC_DIR, "**.cpp"),
    path.join(ZEC_SRC_DIR, "**.h"),
    path.join(ZEC_DIR, "external/src/**.cpp"),
  }

  filter { "files:**.hlsl" }
    flags {"ExcludeFromBuild"}
  filter {}

  flags {
    "FatalWarnings"
  }

  debugenvs { "PATH=%PATH%;../external/lib/;" }
  
  includedirs {
    ZEC_SRC_DIR,
    EXTERNAL_DIR,
  }

  pchheader "pch.h"
  pchsource (path.join(ZEC_SRC_DIR, "pch.cpp"))

  links {
    "dxgi",
    "d3d12",
    "dxguid",
    "D3DCompiler",
    "gainput-d",
    "Xinput9_1_0",
    "ws2_32"
  }

  configuration {}

  include("./tests.lua")

group "examples"
exampleProject(
    "01-hello-world",
    "02-normal-mapping"
)
