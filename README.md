# GROKTASTIC

A polished, original open-world crime/driving sandbox in C++20, CMake and raylib. It takes inspiration from the open-world action genre without copying GTA characters, story, branding, maps or assets.

### Current vertical slice
- Dense procedural city with roads, parks, towers, windows, skyline and street lighting.
- Third-person movement, sprint, jump and mouse-look.
- Enter/exit vehicles and arcade handling with boost and handbrake.
- Ambient traffic and pedestrians.
- Hitscan shooting, pickups, cash, health and armor.
- Escalating wanted level and police cars.
- Three-mission progression loop: Hot Package, Clean Getaway, Heat Check.
- Minimap, objective HUD, crosshair, pause screen and cinematic post-process vignette.
- Procedural visuals mean the project needs no external art pack to run.

### Build
Requirements: CMake 3.20+, Git and a C++20 compiler. Raylib 5.5 is fetched automatically.

```bash
cmake -S . -B build
cmake --build build --config Release
./build/groktastic
```

### Controls
WASD move/drive, mouse camera/aim, Shift sprint/boost, Space jump/handbrake, E enter/exit, left mouse fire, Esc pause.

The code is intentionally a strong vertical slice rather than a misleading claim of AAA completeness. The next production step is to split the systems into renderer, world streaming, AI, vehicle physics, missions and save-game libraries while replacing procedural primitives with authored assets.
