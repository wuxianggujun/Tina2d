# TINA2D

**TINA2D** is a lightweight 2D/2.5D game engine based on [Urho3D](https://github.com/urho3d/Urho3D) with pure BGFX rendering backend.

English | [中文](README_zh.md)

## Overview

TINA2D is a specialized fork of Urho3D focused exclusively on 2D and 2.5D game development. The engine has migrated to **BGFX-only** rendering and removed all OpenGL/D3D legacy code for better performance and cross-platform support.

## Key Features ✅

### **Rendering System**
- **🎯 BGFX-only Backend**: Pure BGFX rendering, OpenGL/D3D paths completely removed
- **🔄 Cross-platform**: Supports DX10/DX11/OpenGL/OpenGL ES/Vulkan through BGFX
- **📦 Complete Shader Pipeline**: 11 optimized shader groups for 2D/2.5D rendering
- **🎨 High-quality Text**: SDF text rendering with anti-aliasing and effects

### **2D/2.5D Capabilities**
- **🌟 2D Core**: Full Urho2D, Physics2D, UI system support
- **💡 2D Lighting**: Point/directional lights with 8-light limit
- **📏 Depth System**: Y→Z mapping for 2.5D depth sorting and occlusion
- **🎭 Advanced Effects**: Post-processing pipeline with framebuffer operations

### **Development Experience**
- **⚡ Streamlined**: Removed 60+ 3D classes and dependencies  
- **📚 Rich Samples**: Optimized 2D sample collection
- **🛠️ Modern Tooling**: CMake-based build with shader compilation
- **📖 Complete Documentation**: Comprehensive technical documentation

## Shader System 🎨

TINA2D includes a complete BGFX shader pipeline with cross-platform support:

### **Shader Naming Convention**
- **Source**: `{name}_vs.sc` / `{name}_fs.sc` (vertex/fragment shaders)
- **Definitions**: `{name}.def.sc` (variable definitions)
- **Compiled**: `{platform}/{name}_vs.bin` / `{name}_fs.bin`

### **Platforms Supported**
- **dx10/dx11**: DirectX 10/11 (Windows)
- **glsl**: OpenGL 3.3+ (Windows/Linux/macOS)
- **essl**: OpenGL ES (Mobile/Web)
- **spirv**: Vulkan (Modern platforms)

### **Shader Location**
```
bin/CoreData/Shaders/BGFX/
├── *.sc                    # Source shaders
├── *.def.sc               # Definition files
├── dx10/*.bin             # DirectX 10 compiled
├── dx11/*.bin             # DirectX 11 compiled
├── essl/*.bin             # OpenGL ES compiled
├── glsl/*.bin             # OpenGL compiled
└── spirv/*.bin            # Vulkan compiled
```

## Build & Development

### **Quick Start**
```bash
# Clone repository
git clone [repository-url] tina2d
cd tina2d

# Generate build files
mkdir build && cd build
cmake ..

# Build (Windows)
cmake --build . --config Release

# Build (Linux/macOS)
make -j$(nproc)
```

### **CMake Options**
- BGFX-only 渲染后端：已默认启用，无需开关
- `URHO3D_2D=ON` (2D features enabled)
- `URHO3D_SAMPLES=ON` (Build sample applications)

### **Development Notes**
- **Shader Compilation**: Automatically handled by CMake
- **Cross-compilation**: Use BGFX's shaderc for custom shaders
- **Asset Pipeline**: Standard Urho3D resource system
## Documentation 📚

- **[2.5D Design Guide](Docs/2_5D_Design.md)** - 2.5D rendering and lighting system
- **[BGFX Migration](Docs/BGFX_Only_Migration.md)** - Complete migration status and assets
- **[Shader Translation Report](Docs/Tina2D_BGFX着色器翻译状态报告.md)** - Shader pipeline details
- **[Integration Guide](Docs/BGFX_2D_INTEGRATION_GUIDE.md)** - Technical integration reference

## License

TINA2D is licensed under the MIT license. Based on [Urho3D](https://github.com/urho3d/Urho3D).

See [LICENSE](licenses/urho3d/LICENSE) for more details.

---

**Status**: 🎯 **Production Ready** - Complete 2D/2.5D game engine  
**Version**: BGFX-only v1.0  
**Last Updated**: 2025-01-22






