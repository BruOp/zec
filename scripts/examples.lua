function exampleProject(...)
  for _, name in ipairs({...}) do
    project("example-" .. name)
    uuid(os.uuid("example-" .. name))
    kind "WindowedApp"

    files {
      path.join(EXAMPLES_DIR, name, "**.cpp"),
      path.join(EXAMPLES_DIR, name, "**.ispc"),
      path.join(EXAMPLES_DIR, name, "**.h")
    }

    removefiles {
      path.join(EXAMPLES_DIR, name, "**.bin.h")
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
      -- "_ITERATOR_DEBUG_LEVEL=0",
    }

    links {
      "zec_lib"
    }

    configuration {"vs*", "x64"}
    linkoptions {
      "/ignore:4199" -- LNK4199: /DELAYLOAD:*.dll ignored; no imports found from *.dll
    }

    configuration { "Release" }
      defines {
        "_ITERATOR_DEBUG_LEVEL=0"
      }

    configuration{}

    DLL_PATH = path.join(EXTERNAL_DIR, "../bin/*.dll")
    postbuildcommands { "cp %{DLL_PATH} %{cfg.targetdir}" }

    filter "files:**.ispc"
      buildmessage "Compiling ISPC files %{file.relpath}"

      buildoutputs {
        "%{cfg.objdir}/%{file.basename}.obj",
        "%{cfg.objdir}/%{file.basename}_sse4.obj",
        "%{cfg.objdir}/%{file.basename}_avx2.obj",
        "%{file.reldirectory}/%{file.basename}_ispc.h"
      }

      filter { "Debug", "files:**.ispc" }
        buildcommands {
          'ispc -g -O0 "%{file.relpath}" -o "%{cfg.objdir}/%{file.basename}.obj" -h "./%{file.reldirectory}/%{file.basename}_ispc.h" --target=sse4,avx2 --opt=fast-math'
        }

      filter { "Release", "files:**.ispc"}
        buildcommands {
          'ispc -O2 "%{file.relpath}" -o "%{cfg.objdir}/%{file.basename}.obj" -h "./%{file.reldirectory}/%{file.basename}_ispc.h" --target=sse4,avx2 --opt=fast-math'
        }

      configuration {}


  end
end
