# TINA2D

**TINA2D** 是一个基于 [Urho3D](https://github.com/urho3d/Urho3D) 的轻量级2D/2.5D游戏引擎，采用纯BGFX渲染后端并集成可视化编辑器。

[English](README.md) | 中文

## 项目概述

TINA2D 是从 Urho3D 游戏引擎分支出来的专门针对2D和2.5D游戏开发的版本。引擎特色包括 **BGFX纯渲染**、**EASTL容器集成** 和 **内置可视化编辑器**，为游戏开发提供流畅的工作流程。

## 主要特性

### **集成可视化编辑器**
- **多面板布局**：层级、检视器、资源浏览器和日志面板
- **场景管理**：可视化场景编辑与节点操作
- **资源管理**：内置资产浏览器和管理功能
- **选择服务**：高级对象选择和检视系统
- **实时更新**：场景和UI的实时更新

### **现代化架构**
- **EASTL集成**：高性能EA标准模板库容器
- **内存管理**：mimalloc优化的自定义分配器
- **STL适配层**：需要时无缝回退到标准容器
- **现代C++17**：先进的语言特性和优化

### **渲染系统**
- **BGFX纯后端**：纯BGFX渲染，完全移除OpenGL/D3D路径
- **跨平台**：通过BGFX支持DX10/DX11/OpenGL/OpenGL ES/Vulkan
- **完整着色器管道**：12组优化的2D/2.5D渲染着色器
- **高质量文本**：SDF和MSDF文本渲染与抗锯齿

### **2D/2.5D能力**
- **2D核心**：完整的Urho2D、Physics2D、UI系统支持
- **2D光照**：增强的Light2D系统，支持点光源/方向光
- **深度系统**：Y→Z映射，用于2.5D深度排序和遮挡
- **高级效果**：带帧缓冲操作的后处理管道
- **丰富示例**：12+个综合2D游戏示例

### **开发体验**
- **精简化**：移除60+个3D类和依赖项
- **内置工具**：集成可视化编辑器，快速开发
- **完整示例**：从基础精灵到复杂平台游戏
- **现代构建**：CMake构建系统，自动着色器编译
- **完整文档**：技术文档和指南

## 着色器系统

TINA2D 包含完整的BGFX着色器管道，支持跨平台：

### **着色器集合**
```
├── Basic2D              # 简单2D渲染
├── Basic_VC             # 顶点颜色基础渲染  
├── Basic_Diff_VC        # 漫反射纹理与顶点颜色
├── Basic_Alpha_VC       # 带顶点颜色的透明混合
├── Basic_DiffAlphaMask_VC # 漫反射与透明遮罩
├── TriangleBatch_VC     # 优化批量渲染
├── Urho2D_VC            # Urho2D顶点颜色着色器
├── Urho2D_Diff_VC       # Urho2D漫反射纹理着色器
├── Urho2D_Lit2D_Diff_VC # 带2D光照的Urho2D
├── Text_SDF_VC          # SDF文本渲染
├── Text_MSDF_VC         # 多通道SDF文本
└── CopyFramebuffer      # 后处理工具
```

### **着色器命名规范**
- **源文件**：`{name}_vs.sc` / `{name}_fs.sc`（顶点/片段着色器）
- **定义文件**：`{name}.def.sc`（变量定义）
- **编译结果**：`{platform}/{name}_vs.bin` / `{name}_fs.bin`

### **支持平台**
- **dx10/dx11**：DirectX 10/11（Windows）
- **glsl**：OpenGL 3.3+（Windows/Linux/macOS）
- **essl**：OpenGL ES（移动设备/Web）
- **spirv**：Vulkan（现代平台）

### **着色器位置**
```
bin/CoreData/Shaders/BGFX/
├── *.sc                    # 源着色器
├── *.def.sc               # 定义文件
├── dx10/*.bin             # DirectX 10编译结果
├── dx11/*.bin             # DirectX 11编译结果
├── essl/*.bin             # OpenGL ES编译结果
├── glsl/*.bin             # OpenGL编译结果
└── spirv/*.bin            # Vulkan编译结果
```

## 集成可视化编辑器

TINA2D 包含内置可视化编辑器，提供流畅的游戏开发体验：

### **编辑器功能**
- **层级面板**：可视化场景图，支持拖拽节点管理
- **检视器面板**：组件属性编辑和配置
- **资源面板**：资产浏览器，支持文件管理
- **日志面板**：实时日志记录和调试信息
- **多面板布局**：可自定义工作区，支持面板停靠

### **开发工作流程**
```
1. 启动编辑器 → 打开/创建场景 → 可视化编辑 → 测试 → 导出
```

编辑器采用原生UI组件实现，提供类似Unity或Godot编辑器的专业游戏开发环境。

## 示例应用

TINA2D 包含全面的2D游戏示例：

### **核心示例**
- **01_HelloWorld** - 基础应用程序设置和窗口管理
- **02_HelloGUI** - UI系统演示，包括按钮和布局
- **03_Sprites** - 基础精灵渲染和动画

### **2D图形与动画**
- **24_Urho2DSprite** - 高级精灵功能和操作
- **25_Urho2DParticle** - 粒子系统效果和配置
- **33_Urho2DSpriterAnimation** - Spriter动画集成
- **47_Typography** - 文本渲染和字体系统
- **48_MSDFText** - 多通道有符号距离场文本
- **51_Urho2DStretchableSprite** - 九宫格精灵实现

### **2D物理与游戏玩法**
- **27_Physics2D** - Box2D物理集成和碰撞
- **28_Physics2DRope** - 高级物理约束和绳索模拟
- **49_Urho2DIsometricDemo** - 带角色移动的等轴2D游戏
- **50_Urho2DPlatformer** - 完整2D平台游戏与角色控制器

### **高级功能**
- **36_Urho2DTileMap** - TMX瓦片地图支持和渲染
- **37_UIDrag** - 高级UI交互和拖拽
- **52_2_5DCore** - 2.5D渲染，包含深度和光照

## 构建与开发

### **快速开始**
```bash
# 克隆仓库
git clone [your-repo] tina2d && cd tina2d

# 生成构建文件
mkdir build && cd build && cmake ..

# 构建（Windows）
cmake --build . --config Release

# 构建（Linux/macOS）
make -j$(nproc)

# 运行可视化编辑器
./bin/Tina2D_Editor

# 运行示例应用
./bin/01_HelloWorld
./bin/52_2_5DCore
```

### **构建要求**
- **CMake 3.15+** - 构建系统
- **C++17编译器** - MSVC 2019+、GCC 9+或Clang 10+
- **Git** - 用于子模块依赖
- **Windows/Linux/macOS** - 跨平台支持

### **CMake配置选项**
```cmake
# 编辑器和工具（推荐）
-DURHO3D_TOOLS=ON              # 构建可视化编辑器和工具

# 示例应用
-DURHO3D_SAMPLES=ON            # 构建所有示例应用

# 2D专项优化（强制）
-DURHO3D_ANGELSCRIPT=OFF       # 为2D专注禁用AngelScript
-DURHO3D_NAVIGATION=OFF        # 禁用导航
-DURHO3D_PHYSICS=OFF           # 禁用3D物理（启用2D物理）
```

### **开发功能**
- **可视化编辑器**：启动`Tina2D_Editor`进行可视化开发
- **着色器编译**：CMake构建系统自动处理
- **热重载**：编辑器支持实时资产和场景更新
- **性能分析**：内置Tracy性能分析器集成
- **调试功能**：全面的日志记录和调试可视化

## 技术架构

### **内存管理**
- **EASTL集成**：具有自定义分配器的高性能EA容器
- **Mimalloc**：为游戏工作负载优化的内存分配
- **智能指针**：现代RAII基于资源管理
- **容器适配**：需要时无缝回退到标准STL

### **渲染管道**
- **纯BGFX**：单一渲染后端消除遗留代码路径
- **跨平台**：所有目标平台统一着色器系统
- **2D优化**：专门针对2D内容的渲染优化
- **现代图形**：通过BGFX支持最新图形API

### **组件架构**
- **节点-组件**：基于组件实体的灵活场景图
- **事件系统**：发布-订阅模式的解耦通信
- **资源系统**：高效的游戏资产缓存和加载
- **输入处理**：键盘、鼠标和触摸的统一输入系统

## 文档

### **开发指南**
- **[2.5D设计指南](Docs/2_5D_Design.md)** - 2.5D渲染和光照系统
- **[BGFX迁移](Docs/BGFX_Only_Migration.md)** - 完整迁移状态和资产
- **[着色器翻译报告](Docs/Tina2D_BGFX着色器翻译状态报告.md)** - 着色器管道详情
- **[集成指南](Docs/BGFX_2D_INTEGRATION_GUIDE.md)** - 技术集成参考

### **API参考**
- **编辑器API** - 可视化编辑器集成和扩展点
- **渲染API** - BGFX集成和着色器系统
- **2D系统** - 精灵、物理、动画和UI系统
- **资产管道** - 资源管理和加载

## 快速入门

### **1. 构建引擎**
```bash
git clone [你的仓库] tina2d && cd tina2d
mkdir build && cd build && cmake .. && cmake --build . --config Release
```

### **2. 启动可视化编辑器**
```bash
./bin/Tina2D_Editor
```

### **3. 尝试示例应用**
```bash
./bin/01_HelloWorld         # 基础应用
./bin/50_Urho2DPlatformer   # 完整2D游戏
./bin/52_2_5DCore           # 2.5D功能
```

### **4. 开始你的项目**
- 在编辑器中创建新场景
- 添加2D精灵、物理体、UI元素
- 配置光照和效果
- 构建并部署你的游戏

## 社区与支持

### **项目状态**
- **生产就绪** - 稳定的2D/2.5D游戏开发
- **积极开发** - 定期更新和改进
- **文档完善** - 全面的指南和示例
- **实战测试** - 多个示例游戏和用例

## 许可证

TINA2D 基于 MIT 许可证发布，基于 [Urho3D](https://github.com/urho3d/Urho3D)。

详情请参见 [LICENSE](licenses/urho3d/LICENSE)。

---

**TINA2D** - 专业2D/2.5D游戏引擎与可视化编辑器  
**版本**: BGFX-only v2.0 集成编辑器版  
**更新日期**: 2025-09-25  
**状态**: 生产就绪 - 立即开始制作精彩的2D游戏！
