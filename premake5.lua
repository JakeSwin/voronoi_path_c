workspace "VoroPathplanner"
configurations { "Debug", "Release" }

project "VoroPathplanner"
kind "ConsoleApp"
language "C"
targetdir "bin/%{cfg.buildcfg}"

files { "./src/**.h", "./src/**.c" }

filter "system:linux"
links { "m", "raylib" }

filter "system:macosx"
links { "m", "raylib", "Cocoa.framework", "IOKit.framework", "OpenGL.framework" }

filter "configurations:Debug"
defines { "DEBUG" }
symbols "On"

filter "configurations:Release"
defines { "NDEBUG" }
optimize "On"
