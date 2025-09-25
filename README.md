# TINA2D

**TINA2D** is a lightweight 2D/2.5D game engine based on [Urho3D](https://github.com/urho3d/Urho3D) with pure BGFX rendering backend and integrated visual editor.

English | [中文](README_zh.md)

## Overview

TINA2D is a specialized fork of Urho3D focused exclusively on 2D and 2.5D game development. The engine features **BGFX-only** rendering, **EASTL container integration**, and a **built-in visual editor** for streamlined game development workflow.

## Key Features ✅

### **Integrated Visual Editor** 🎨
- **Multi-Panel Layout**: Hierarchy, Inspector, Resource Browser, and Log panels
- **Scene Management**: Visual scene editing with node manipulation
- **Resource Management**: Built-in asset browser and management
- **Selection Service**: Advanced object selection and inspection system
- **Live Updates**: Real-time scene and UI updates

### **Modern Architecture** ⚡
- **EASTL Integration**: High-performance EA Standard Template Library containers
- **Memory Management**: Custom allocators with mimalloc optimization
- **STL Adapter Layer**: Seamless fallback to standard containers when needed
- **Modern C++17**: Advanced language features and optimizations

### **Rendering System**
- **🎯 BGFX-only Backend**: Pure BGFX rendering, OpenGL/D3D paths completely removed
- **🔄 Cross-platform**: Supports DX10/DX11/OpenGL/OpenGL ES/Vulkan through BGFX
- **📦 Complete Shader Pipeline**: 12 optimized shader groups for 2D/2.5D rendering
- **🎨 High-quality Text**: SDF and MSDF text rendering with anti-aliasing

### **2D/2.5D Capabilities**
- **🌟 2D Core**: Full Urho2D, Physics2D, UI system support  
- **💡 2D Lighting**: Enhanced Light2D system with point/directional lights
- **📏 Depth System**: Y→Z mapping for 2.5D depth sorting and occlusion
- **🎭 Advanced Effects**: Post-processing pipeline with framebuffer operations
- **🎮 Rich Samples**: 12+ comprehensive 2D game examples

### **Development Experience**
- **⚡ Streamlined**: Removed 60+ 3D classes and dependencies
- **🛠️ Built-in Tools**: Integrated visual editor for rapid development
- **📚 Complete Examples**: From basic sprites to complex platformers
- **🏗️ Modern Build**: CMake-based build with automated shader compilation
- **📖 Comprehensive Docs**: Technical documentation and guides

## Shader System 🎨

TINA2D includes a complete BGFX shader pipeline with cross-platform support:

### **Shader Collection**
```
├── Basic2D              # Simple 2D rendering
├── Basic_VC             # Vertex color basic rendering  
├── Basic_Diff_VC        # Diffuse texture with vertex color
├── Basic_Alpha_VC       # Alpha blending with vertex color
├── Basic_DiffAlphaMask_VC # Diffuse with alpha mask
├── TriangleBatch_VC     # Optimized batch rendering
├── Urho2D_VC            # Urho2D vertex color shader
├── Urho2D_Diff_VC       # Urho2D diffuse texture shader
├── Urho2D_Lit2D_Diff_VC # Urho2D with 2D lighting
├── Text_SDF_VC          # SDF text rendering
├── Text_MSDF_VC         # Multi-channel SDF text
└── CopyFramebuffer      # Post-processing utilities
```

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

## Integrated Visual Editor 🛠️

TINA2D includes a built-in visual editor for streamlined game development:

### **Editor Features**
- **📁 Hierarchy Panel**: Visual scene graph with drag-drop node management
- **🔍 Inspector Panel**: Component properties editing and configuration
- **📂 Resource Panel**: Asset browser with file management capabilities  
- **📝 Log Panel**: Real-time logging and debugging information
- **🎨 Multi-Panel Layout**: Customizable workspace with dockable panels

### **Development Workflow**
```
1. Launch Editor → Open/Create Scene → Edit Visually → Test → Export
```

The editor is implemented with native UI components and provides a professional game development environment similar to Unity or Godot editors.

## Sample Applications 🎮

TINA2D includes comprehensive 2D game examples:

### **Core Samples**
- **01_HelloWorld** - Basic application setup and window management
- **02_HelloGUI** - UI system demonstration with buttons and layouts  
- **03_Sprites** - Basic sprite rendering and animation

### **2D Graphics & Animation**
- **24_Urho2DSprite** - Advanced sprite features and manipulation
- **25_Urho2DParticle** - Particle system effects and configuration
- **33_Urho2DSpriterAnimation** - Spriter animation integration
- **47_Typography** - Text rendering and font systems
- **48_MSDFText** - Multi-channel signed distance field text
- **51_Urho2DStretchableSprite** - 9-patch sprite implementation

### **2D Physics & Gameplay**  
- **27_Physics2D** - Box2D physics integration and collision
- **28_Physics2DRope** - Advanced physics constraints and rope simulation
- **49_Urho2DIsometricDemo** - Isometric 2D game with character movement
- **50_Urho2DPlatformer** - Complete 2D platformer with character controller

### **Advanced Features**
- **36_Urho2DTileMap** - TMX tilemap support and rendering
- **37_UIDrag** - Advanced UI interactions and drag-drop
- **52_2_5DCore** - 2.5D rendering with depth and lighting

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

# Run the visual editor
./bin/Tina2D_Editor

# Run sample applications
./bin/01_HelloWorld
./bin/52_2_5DCore
```

### **Requirements**
- **CMake 3.15+** - Build system
- **C++17 Compiler** - MSVC 2019+, GCC 9+, or Clang 10+
- **Git** - For submodule dependencies
- **Windows/Linux/macOS** - Cross-platform support

### **CMake Configuration Options**
```cmake
# Editor and tools (recommended)
-DURHO3D_TOOLS=ON              # Build visual editor and tools

# Sample applications  
-DURHO3D_SAMPLES=ON            # Build all sample applications

# 2D-only optimizations (forced)
-DURHO3D_ANGELSCRIPT=OFF       # AngelScript disabled for 2D focus
-DURHO3D_NAVIGATION=OFF        # Navigation disabled
-DURHO3D_PHYSICS=OFF           # 3D Physics disabled (2D Physics enabled)
```

### **Development Features**
- **🎨 Visual Editor**: Launch `Tina2D_Editor` for visual development
- **📦 Shader Compilation**: Automatically handled by CMake build system  
- **🔧 Hot Reload**: Editor supports live asset and scene updates
- **📊 Performance**: Built-in Tracy profiler integration
- **🐛 Debugging**: Comprehensive logging and debug visualization
## Technical Architecture 🏗️

### **Memory Management**
- **EASTL Integration**: High-performance EA containers with custom allocators
- **Mimalloc**: Optimized memory allocation for gaming workloads
- **Smart Pointers**: Modern RAII-based resource management
- **Container Adaptation**: Seamless fallback to standard STL when needed

### **Rendering Pipeline**
- **Pure BGFX**: Single rendering backend eliminates legacy code paths
- **Cross-Platform**: Unified shader system across all target platforms  
- **Optimized 2D**: Specialized rendering optimizations for 2D content
- **Modern Graphics**: Support for latest graphics APIs through BGFX

### **Component Architecture**
- **Node-Component**: Flexible scene graph with component-based entities
- **Event System**: Publisher-subscriber pattern for decoupled communication
- **Resource System**: Efficient caching and loading of game assets
- **Input Handling**: Unified input system for keyboard, mouse, and touch

## Documentation 📚

### **Development Guides**
- **[2.5D Design Guide](Docs/2_5D_Design.md)** - 2.5D rendering and lighting system
- **[BGFX Migration](Docs/BGFX_Only_Migration.md)** - Complete migration status and assets  
- **[Shader Translation Report](Docs/Tina2D_BGFX着色器翻译状态报告.md)** - Shader pipeline details
- **[Integration Guide](Docs/BGFX_2D_INTEGRATION_GUIDE.md)** - Technical integration reference

### **API Reference**
- **Editor API** - Visual editor integration and extension points
- **Rendering API** - BGFX integration and shader system  
- **2D Systems** - Sprite, physics, animation, and UI systems
- **Asset Pipeline** - Resource management and loading

## Getting Started 🚀

### **1. Build the Engine**
```bash
git clone [your-repo] tina2d && cd tina2d
mkdir build && cd build && cmake .. && cmake --build . --config Release
```

### **2. Launch Visual Editor**
```bash
./bin/Tina2D_Editor
```

### **3. Try Sample Applications**
```bash  
./bin/01_HelloWorld         # Basic application
./bin/50_Urho2DPlatformer   # Complete 2D game
./bin/52_2_5DCore           # 2.5D features
```

### **4. Start Your Project**
- Create new scene in editor
- Add 2D sprites, physics bodies, UI elements
- Configure lighting and effects  
- Build and deploy your game

## Community & Support 💬

### **Project Status**
- **🎯 Production Ready** - Stable for 2D/2.5D game development
- **🔄 Active Development** - Regular updates and improvements  
- **📖 Well Documented** - Comprehensive guides and examples
- **🎮 Battle Tested** - Multiple sample games and use cases

### **Getting Help**
- **Documentation** - Start with the comprehensive docs above
- **Sample Code** - 12+ working examples for every feature
- **Issue Tracker** - Report bugs and request features  
- **Community** - Join discussions with other developers

## License

TINA2D is licensed under the MIT license. Based on [Urho3D](https://github.com/urho3d/Urho3D).

See [LICENSE](licenses/urho3d/LICENSE) for more details.

---

**⭐ TINA2D** - Professional 2D/2.5D Game Engine with Visual Editor  
**🎮 Version**: BGFX-only v2.0 with Integrated Editor  
**📅 Last Updated**: 2025-01-25  
**🔧 Status**: Production Ready - Build Amazing 2D Games Today!






