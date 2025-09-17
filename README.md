# TINA2D

**TINA2D** is a lightweight 2D game engine based on [Urho3D](https://github.com/urho3d/Urho3D).

English | [中文](README_zh.md)

## Overview

TINA2D is a fork of the Urho3D game engine specifically designed for 2D game development. All 3D-related functionality has been removed to provide an efficient and streamlined 2D game development experience.

## Key Changes

- **Removed 3D functionality**: Eliminated 60+ 3D-related classes and the entire 3D rendering pipeline
- **Retained 2D core**: Preserved all Urho2D, Physics2D, UI system, and other 2D features
- **SDL3 upgrade**: Upgraded the underlying SDL library from SDL2 to SDL3 for better performance and new features
- **Streamlined build**: Removed dependencies including AngelScript, IK, Navigation, and 3D Physics
- **Optimized samples**: Kept and optimized 2D-related samples while removing 3D examples

## Based on Open Source Project

This project is based on the [Urho3D](https://github.com/urho3d/Urho3D) game engine, a powerful cross-platform 2D and 3D game engine.

## Features

- **2D-focused**: Pure 2D game engine without 3D functionality overhead
- **High performance**: Modern graphics backend based on SDL3
- **Rich 2D features**: Sprite rendering, particle systems, tile maps, physics engine
- **Easy to use**: Simplified API with extensive sample code
- **Cross-platform**: Supports Windows, Linux, macOS, and other platforms
- **Lightweight**: Significantly reduced size and dependencies compared to full Urho3D

## Build Requirements

- CMake 3.15+
- C++17 compatible compiler
- SDL3 library (included)

## Quick Start

```bash
# Clone the project
git clone [your-repository-url]
cd Tina2d

# Build the project
mkdir build && cd build
cmake ..
cmake --build .
```

## Sample Projects

TINA2D includes multiple 2D game samples:

- **Sprite Rendering** - 2D sprite display and animation
- **Particle Systems** - 2D particle effects
- **Physics Simulation** - 2D physics collision and constraints
- **Tile Maps** - TMX format map support
- **UI System** - Complete user interface components

## Technical Architecture

TINA2D retains Urho3D's core architecture but is specifically optimized for 2D:

- **Renderer**: SDL3-based 2D rendering pipeline
- **Scene System**: Node-component architecture
- **Resource Management**: Efficient resource caching system
- **Event System**: Publisher-subscriber pattern event handling
- **Input Processing**: Keyboard, mouse, and touch input support

## License

TINA2D is licensed under the MIT License. See [LICENSE](licenses/urho3d/LICENSE) for details.

Third-party library licenses: [licenses](licenses)

Credits: [licenses/urho3d/credits.md](licenses/urho3d/credits.md)

## Related Links

### Original Project
* [Urho3D Official Repository](https://github.com/urho3d/Urho3D)
* [Urho3D Documentation](https://urho3d-doxygen.github.io/latest/index.html)
* [Urho3D Community Forum](https://github.com/urho3d-community/discussions/discussions)

### SDL3 Related
* [SDL3 Official Documentation](https://wiki.libsdl.org/SDL3/FrontPage)
* [SDL3 Migration Guide](https://github.com/libsdl-org/SDL/blob/main/docs/README-migration.md)

## Project Authors

**TINA2D** is based on the [Urho3D](https://github.com/urho3d/Urho3D) project.

Urho3D was founded by [Lasse Öörni](https://github.com/cadaver). Huge contributions were also made by [Yao Wei Tjong](https://github.com/weitjong), [asterj](https://github.com/aster2013), and [many other contributors](https://github.com/urho3d/Urho3D/graphs/contributors).

The 2D-specific optimizations and SDL3 upgrade for TINA2D were completed by the project maintenance team.
