# After the Rain: Streetlight Swarm

A C++17 / raylib 3D simulation of a monsoon-night Indian street, with moth-like agents flying in a 3D volume, a sequential streetlight chase, and two identical moving buses on separate lanes. The layout follows Assignment 2: `src/`, `build/`, `include/`, and `lib/`.

## Build on Windows (MinGW)

From the Assignment 3 directory in PowerShell:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -o "build\StreetlightSwarm.exe" "src\main.cpp" -I"include" -L"lib" -lraylib -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32
Copy-Item "lib\raylib.dll" "build\raylib.dll" -Force
```

Run with `./build/StreetlightSwarm.exe`.

The CMake configuration mirrors the prior assignment:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Controls

- Before starting, drag the population slider or press `N` and enter an exact count (30-1000).
- Set the automatic light cycle between 10 and 60 seconds (unused in Manual Lights mode).
- Choose one of three modes: `Cycle Lights` (moths seek periodically switching lamps), `Manual Lights` (you toggle lamps), or `No Attraction` (lamps cycle as scenery, but moths ignore them).
- Click **Start Simulation** to apply those settings and begin.
- In Manual Lights mode, click the four lamp switches in the upper-left panel. The other modes do not expose runtime light switching.
- Left mouse drag: orbit camera; right drag: pan; wheel: zoom.
- `O`: show/hide the two buses; Space: pause; `R`: return to setup.

The 3D spatial hash gathers nearby agents in adjacent cells. The force update combines 3D light attraction, separation, alignment, cohesion, wind response, and look-ahead avoidance of the moving vehicles. Accelerations are computed from the same pre-integration state; positions are stepped with semi-implicit Euler.

In cycle modes, one lamp is lit at a time in a clear first-to-last repeating sequence, so the flock follows a moving target. The two matching buses move along separate parallel lanes with non-overlapping bounds. The scene contains only the street, lamps, rain, buses, and moths. Rain streaks fall downward in the Y-up world. raylib handles 3D rendering, camera input, and the window; the custom spatial grid supplies local neighbor candidates.

The moth flight volume is kept close to the roadway and lamp row. No Attraction mode keeps the flock at a lower ceiling, and combines look-ahead steering with a solid-bus overlap correction so moths do not pass through the vehicles at high population counts.
