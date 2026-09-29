# Interactive 3D Vending Machine

**Student Name:** Md. Nayeem  
**Roll Number:** 2107050  
**Course:** Computer Graphics / Image Processing Lab  
**Framework:** OpenGL 3.3 (Core Profile), GLAD, GLFW  

---

## 🎯 Project Overview

This project presents a fully interactive 3D vending machine simulation placed inside a detailed room environment, built entirely from fundamental geometric primitives using 3D transformations (`translate`, `rotate`, `scale`).

The application demonstrates:
- **Hierarchical Modeling**: Complex composite objects (cans, bottles, snack pouches, candy bars, vending cabinet, shelves, helical spirals, trash bin, furniture, potted plant) built from basic shapes (cubes, cylinders, spheres, coils, and quads).
- **Three Types of Lighting** (Blinn-Phong model):
  1. **Directional Light**: Simulates ambient room daylight / overhead lighting.
  2. **Point Light**: Interior fluorescent cabinet tube light with distance attenuation.
  3. **Spot Light**: Overhead ceiling track spotlight focused on the vending machine with inner and outer cutoff cones.
  - Plus a dynamic collection bay beacon light!
- **Dispensing Physics & Animation**:
  - Rotating product spirals that move items forward along the shelf.
  - Simulated gravity drop, realistic tumbling rotations, and damped bouncing upon landing into the collection tray.
- **Hierarchical Collection Door**:
  - Hinged push flap with "PUSH" plate that rotates around its top hinge axis.
  - Opens automatically when items drop, and can be manually opened/closed.
- **Interactive Buttons & Indicator Lights**:
  - Tactile keypad buttons that visibly depress into the panel when pressed.
  - Dynamic indicator LEDs (Ready Green LED, Vending Amber LED, Collection Blue LED).
  - Glowing LED status screen and illuminated marquee header sign.
- **Comprehensive Camera System**:
  - Smooth Mouse Orbit (Left-drag), Pan (Right-drag), and Zoom (Scroll / W, S).
  - 5 Preset Camera Views: Room Overview, Machine Front, Shelves Close-up, Keypad Panel, and Collection Door.
  - Turntable Auto-Rotation mode.

---

## 🎮 Controls & Interactions

| Key / Input | Action |
| :--- | :--- |
| **`[1]` to `[9]`** | Select and vend product in slot 1 to 9 |
| **`[0]`** | Select and vend product in slot 10 |
| **`[SPACE]`** | Vend next / random product |
| **`[O]`** | Toggle Open / Close Collection Door flap |
| **`[D]` or `[F1]`** | Toggle **Light Type 1**: Directional Room Sunlight |
| **`[P]` or `[F2]`** | Toggle **Light Type 2**: Interior Point Light |
| **`[S]` or `[F3]`** | Toggle **Light Type 3**: Ceiling Spotlight |
| **`[V]`** | Cycle Camera Presets *(Room -> Front -> Shelves -> Keypad -> Tray)* |
| **`[C]`** | Toggle Turntable Auto-Rotation |
| **`[R]`** | Reset Camera to Room Overview |
| **Mouse Left Drag** | Orbit Camera (Yaw / Pitch) |
| **Mouse Right Drag** | Pan Camera Target |
| **Mouse Scroll** | Zoom In / Out |
| **`[ESC]`** | Exit Application |

---

## 📁 Project Structure

```text
opengl-project/
├── include/
│   ├── basic_shapes.h       # Procedural unit meshes (Cube, Cylinder, Sphere, Coil, Plane)
│   ├── camera.h             # 3D Orbit / Pan / Zoom / Preset Camera system
│   ├── lighting.h           # Directional, Point, Spot light and Material structures
│   ├── math3d.h             # Self-contained Vec2, Vec3, Vec4, Mat4 & transformation math
│   ├── glad/gl.h            # OpenGL loader
│   ├── GLFW/glfw3.h         # Windowing and input handling
│   └── KHR/khrplatform.h
├── lib/
│   ├── libglfw3.a
│   └── glfw3.dll
├── src/
│   ├── glad.c               # GLAD implementation
│   └── main.cpp             # Complete scene assembly, hierarchical models, physics, and rendering
├── graphics project.pdf     # Academic assignment specifications
├── run.bat                  # 1-click build and run script
└── README.md
```

---

## 🚀 How to Build and Run

### 1-Click Execution (Recommended)
Simply run or double-click `run.bat`:
```cmd
run.bat
```

### Manual Command Line Build
```bash
g++ -std=c++17 src/main.cpp src/glad.c -Iinclude -Llib -lglfw3 -lopengl32 -lgdi32 -o main.exe
./main.exe
```
