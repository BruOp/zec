function exampleProject(...)
  for _, name in ipairs({...}) do
    project("example-" .. name)
    uuid(os.uuid("example-" .. name))
    kind "WindowedApp"
    architecture "x64"

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
      ZEC_SRC_DIR,
      ASSET_LIB_DIR
    }

    flags {
      "FatalWarnings"
    }

    defines {
      "_SECURE_SCL=0",
    }

    links {
      "zec_lib",
      "asset_lib"
    }

    filter { "configurations:Release" }
      defines {
        "_ITERATOR_DEBUG_LEVEL=0"
      }
    filter{}

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

    filter {}
  end
end
