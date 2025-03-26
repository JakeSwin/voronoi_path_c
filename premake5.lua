workspace "VoroPathplanner"
configurations { "Debug", "Release" }

project "VoroPathplanner"
kind "ConsoleApp"
language "C"
targetdir "bin/%{cfg.buildcfg}"

files { "./src/**.h", "./src/**.c" }

filter "system:linux"
links { "m" }   -- Link math library if jc_voronoi uses math functions

filter "configurations:Debug"
defines { "DEBUG" }
symbols "On"

filter "configurations:Release"
defines { "NDEBUG" }
optimize "On"
