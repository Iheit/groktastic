# GROKTASTIC

A small, original, open-world crime/driving sandbox built in modern C++20 with CMake and raylib. It is **inspired by the open-world action genre**, not a recreation of any copyrighted game or setting.

## What is already here

- Procedural city blocks, roads, parks, towers, lamps, storefronts, traffic and skyline dressing.
- Third-person character controller with sprinting, jumping and camera-relative movement.
- Enter/exit vehicles, arcade driving, handbrake, headlights and vehicle damage.
- Traffic simulation with simple lane following and avoidance.
- Pedestrians with wandering behavior.
- Aim/shoot loop with hitscan weapons, muzzle flashes and enemy reactions.
- Wanted system with escalating police response.
- Mission chain: **Hot Package**, **Clean Getaway**, **Heat Check**.
- Pickups, cash, health, armor and a simple progression loop.
- Minimap, objective marker, wanted stars, mission banner, crosshair and cinematic letterbox effects.
- Post-processing-style vignette/color grading shader and dynamic lighting accents.
- Fully procedural visuals: no external art assets are required.

## Build

Requirements: CMake 3.20+, a C++20 compiler, and Git. Raylib 5.5 is fetched automatically by CMake.

```bash
cmake -S . -B build
cmake --build build --config Release
./build/groktastic
```

On multi-config generators, run the executable from the generated Release directory.

## Controls

- **WASD**: move / drive
- **Mouse**: camera / aim
- **Shift**: sprint / boost
- **Space**: jump / handbrake
- **E**: enter or exit vehicle
- **Left mouse**: fire
- **R**: reload
- **Esc**: release mouse / quit

## Design target

The project deliberately favors a dense, attractive, playable vertical slice over pretending a tiny repository can contain a AAA production. The architecture is intentionally easy to grow: world generation, simulation, missions, combat, vehicles, UI and rendering are separated into small systems inside `src/main.cpp` so the next pass can split them into libraries without changing gameplay.
