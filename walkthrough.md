# Singularity Visualization Walkthrough

## Controls

### General
- **ESC**: Exit the application.
- **TAB**: Cycle through Color Maps (Grayscale, Red/Blue, Red/Blue/Abs, etc.).
- **BACKSPACE**: Cycle through Normalization Maps (Linear, Logarithmic, Log-Steps).
- **UP / DOWN**: Increase/Decrease quantization steps (for Log-Steps normalization).
- **G**: Toggle "Grab/Move Mode".
- **Ctrl + R**: Respawn poles and zeros randomly.

### Interaction (Grab Mode)
When in Grab Mode (Press **G**, cursor changes to Hand):
1.  **Hover** over a Pole (Red center) or Zero (Blue center).
2.  **Click and Drag** to move the entity.
3.  Release to drop.
4.  The visualization updates in real-time.

## Features
- **Structure of Arrays (SoA)**: The engine uses SoA layout for maximum performance and SIMD efficiency.
- **Dynamic Color Inversion**: Guidelines ring colors invert based on background for visibility.
- **Anti-Aliased Rendering**: Smooth circles and lines.
- **Borderless Fullscreen**: Supports Alt-Tab multitasking.
