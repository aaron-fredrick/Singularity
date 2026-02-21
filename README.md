# Singularity

Singularity is a high-performance **S-plane visualizer** and experimental music instrument that translates complex transfer functions, formed by the interaction of **poles and zeros**, into a symphony of color and sound.

## Purpose & Inspiration

This project is a tribute to the logical elegance of **Laplace Transformations** and control theory. It provides a real-time, interactive window into the **S-Domain**, where the stability and behavior of linear systems are dictated by the landscape of singularities.

Influenced by the work and spirit of the late developer **Terry Davis**, Singularity was built with a dedication to first-principles development and the creation of unique, personal digital artifacts. 

The goal is to move beyond the textbook definitions of transfer functions. It allows users to **see the colors of math** and explore the infinite intricacy of the complex plane. By mapping mathematical magnitude and phase to vibrant visual gradients and synthetic soundscapes, we turn the screen into an auditory and visual representation of the **Laplace landscape**.

## Features

- **High-Performance Math Engine**: Built in C with a Structure of Arrays (SoA) layout for maximum SIMD efficiency.
- **Dynamic Sonification**: The field magnitude and phase are mapped to a procedurally generated synthesizer, turning the visualizer into a playable instrument.
- **Advanced Visual Overlays**:
    - **Phase Trails**: Real-time vector field streamlines tracing the flow of the complex plane.
    - **Domain Coloring**: A warped checkerboard pattern illustrating the transformation of space.
- **Cinematic Experience**: Supports auto-orbit, pulsing animations, and high-resolution screenshot export.
- **Cross-Platform**: Compiles natively on Windows (MinGW) and Linux (Ubuntu).

## Usage & Controls

### General Navigation
| Key | Action |
|---|---|
| `Alt + Enter` | Toggle Fullscreen |
| `Escape` | Exit Application |
| `H` | Toggle HUD Overlays |
| `S` / `F1` | Open Settings Panel |
| `P` | Export Screenshot (saved as `.bmp`) |
| `Ctrl + R` | Randomize all singularities |
| `Ctrl + Z / Y` | Undo / Redo changes |

### Interaction & Manipulation
| Input | Action |
|---|---|
| `G` | Toggle Select ↔ Move Mode |
| `Z` / `X` | Add Zero / Pole at cursor (Move Mode) |
| `Delete` | Remove selected singularity |
| `Mouse Wheel` | Zoom in/out at cursor position |
| `Middle Click Drag` | Pan around the complex plane |
| `Right Click Drag` | Drag to select a group of singularities |
| `1` - `9` | Load mathematical presets (Ring, Elliptic, Cross, etc.) |

### Visual & Audio Effects
| Key | Action |
|---|---|
| `Tab` | Cycle through 6 distinct Color Maps |
| `Backspace`| Cycle Normalization (Linear, Log, Steps) |
| `C` | Cycle Entity Rendering (Solid, Transparent, Dashed, etc.) |
| `O` | Toggle Auto-Orbit Animation |
| `U` | Toggle Magnitude Pulsation |
| `F` | Toggle Vector Field / Phase Trails |
| `D` | Toggle Domain Coloring Overlay |
| `A` | **Toggle Audio Sonification** (Theremin Mode) |

## Building

### Windows (MinGW/MSYS2)
Ensure you have `gcc`, `make`, and `SDL2` installed via MSYS2.
```bash
make
./bin/singularity.exe
```

### Linux (Ubuntu)
```bash
sudo apt install libsdl2-dev build-essential
make
./bin/singularity
```

---

*Visualization is not just about seeing; it's about understanding the rhythmic beauty of thought.*
