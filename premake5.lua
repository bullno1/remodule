workspace "remodule"
  location(_ACTION)
  configurations { "Debug", "Release" }
  architecture "x86_64"
  filter {"system:windows", "action:vs*"}
    systemversion("10.0.22621.0")

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
