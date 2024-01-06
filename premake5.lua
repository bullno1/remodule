workspace "remodule"
  configurations { "Debug", "Release" }
  location(_ACTION)

  warnings "Extra"
  flags {
    "FatalWarnings",
  }

  filter { "action:vs*" }
    buildoptions {
      "/std:c11",
    }
    disablewarnings {
      "4100",
      "4200",
      "4152",
    }

  debugdir "bin/%{cfg.buildcfg}"

project "host"
  kind "ConsoleApp"
  language "C"
  targetdir "bin/%{cfg.buildcfg}"

  files {
   "remodule.h",
   "remodule_monitor.h",
   "example_host.c",
  }

  filter "configurations:Debug"
    defines { "DEBUG" }
    symbols "On"

  filter "configurations:Release"
    defines { "NDEBUG" }
    optimize "On"

project "plugin"
  kind "SharedLib"
  language "C"
  targetdir "bin/%{cfg.buildcfg}"

  files {
    "remodule.h",
    "example_plugin.c",
  }

  filter "configurations:Debug"
    defines { "DEBUG" }
    symbols "On"

  filter "configurations:Release"
    defines { "NDEBUG" }
    optimize "On"
