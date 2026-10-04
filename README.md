# Jungle Rest Stop — OpenGL 3.3 Computer Graphics Project

**Student:** Md. Nayeem  **Roll:** 2107050  **Course:** Computer Graphics / Image Processing Lab
**Tech:** C++17, OpenGL 3.3 Core, GLFW, GLAD. No model files and no image files are loaded.
Every object is made from basic shapes, and every texture is generated in code.

A rest stop deep in a jungle. Visitors arrive **only by car**. They park, walk inside and use
the three connected rooms, then drive away. You walk around as a person and can use every machine.

| Room | Contents |
|---|---|
| **Main lounge** | sofas and armchairs, wall TV with 4 animated channels, coffee machine, **3 fully working vending machines** (drinks, snacks, sweets) |
| **ATM room** | ATM that shows each step on its screen and in a HUD panel: card → PIN → service → amount → processing → card → cash → receipt |
| **Library** | bookshelves, reading table, armchairs, floor lamp, **aquarium** with swimming fish (you can feed them) |
| **Outside** | parking lot (20 spots), road through a procedural jungle with rainforest trees, palms, bamboo, banana plants, ferns, grass, flowers, rocks, logs, vines, birds and fireflies |

## Features
* **Basic ↔ Enhanced switch.** Press **B**, or click the on-screen button.
  *Basic* draws plain colours with simple lighting.
  *Enhanced* adds procedural textures, shadow mapping with PCF, fog, a sky with clouds and stars, wind animation and tone mapping.
* **Day and night cycle** with a moving sun and moon, sunrise and sunset colours, and street lamps that switch on automatically.
* **Many light types:** sun or moon (directional), bulbs and street lamps (point), downlights, the reading lamp and car headlights (spot), glowing screens and LEDs (emissive), and sky/ground ambient light. Wall light switches work.
* **Cameras:** first person, third person and free fly camera. The mouse wheel changes the **FOV**.
* **Performance:** objects are drawn with instancing, the world is split into chunks with frustum and distance culling, and each room only uses its own lights. The frame rate is capped at **60 or 30 FPS**.
* **NPC AI:** cars follow road paths and Bézier parking curves. Only one car moves in the lot at a time. People walk the shortest path on a waypoint graph (Dijkstra) and use the machines through the same code the player uses.

## Build and run (Windows, MinGW-w64 / MSYS2 g++)
```bat
run.bat                 :: compile everything and start
run.bat --low           :: low quality (for very weak PCs)
```
Other ways to build: `./build.sh` (fast incremental build in Git Bash), `make`, or CMake.
Command-line options for `main.exe`:

| Option | Meaning |
|---|---|
| `--low` | 1024 shadow map, fewer plants, no MSAA |
| `--fullscreen` | full screen |
| `--size W H` | window size |
| `--shots DIR` | renders every report screenshot, object build-stage image, texture swatch and benchmark into `DIR`, then exits |

## Controls
| Key | Action |
|---|---|
| W A S D / Shift / Space | move / run / jump (free camera: Space and Ctrl move up and down) |
| Mouse | look |
| **E** or Left click | use the thing in the crosshair (buttons, slots, flap, switches, TV, aquarium) |
| **V** | first person → third person → free camera |
| Mouse wheel, + / − | change FOV |
| **B** | **Basic ↔ Enhanced** rendering |
| T / G / F | textures / shadows / fog on or off |
| **N** | jump between day and night; `[` `]` change time speed; M pauses time |
| R | roof cut-away (see inside from above) |
| L | FPS cap 60 ↔ 30 |
| K | call a visitor car now |
| F1–F4 | lounge / spot / ATM + library / outdoor lights |
| F5 | hide HUD; **F12** screenshot (saved to `screenshots/`) |
| Alt | free the mouse so you can click the HUD buttons |
| H | help; Esc quits |

**Vending machine:** insert money (Tk 10 coin or Tk 50 note), type a code such as `B2`, press **OK**, then push the flap to take the item. Collect your change from the coin cup.
**ATM:** insert your card, enter PIN **1234**, choose a service and an amount with the side keys, then take your card, cash and receipt.

## Project structure
```text
src/
  core/     Math3D.h          vectors, matrices, transforms, frustum, noise, random
  gfx/      Shapes            the basic shapes (cube, cylinder, sphere, cone, torus, wedge ...)
            Draw.h            Material + hierarchical Node (parent * T * R * S)
            Renderer          instancing, static chunks, shadow map, sky, transparency
            Textures          procedural texture array (grass, brick, wood, leaves ...)
            Text, Shader, Screenshot
  scene/    Layout            positions of everything, terrain height, road curve
            Building          walls with windows/doors, floors, lot, road, lamps, sliding doors
            Furniture         sofas, shelves, lamps, clocks, plants, benches ...
            Nature            jungle generator + birds and fireflies
            Lights, Materials
  objects/  VendingMachine, ATM, Appliances (coffee machine, TV, aquarium), Products
  actors/   Human (articulated person), Car, Visitors (NPC AI)
  game/     Player (cameras), Interaction (ray picking), Collision, DayNight, Hud, Inventory
  app/      App (main loop, input, frame cap), Showcase (report screenshots)
report/     LaTeX report (report.tex) + figures
legacy/     the original single-file project (kept for comparison)
```

## Report
The full LaTeX report is in [`report/report.tex`](report/report.tex), and the compiled version is `report/report.pdf`.
It explains how every object is built from basic shapes, and covers lighting, texturing, shadows, animation, the NPC logic and performance, with screenshots.
To rebuild the figures, run `main.exe --shots report/shots`, then `python report/make_figures.py`, then `pdflatex report.tex` twice.
