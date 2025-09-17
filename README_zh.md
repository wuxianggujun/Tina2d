# TINA2D

**TINA2D** 是一个基于 [Urho3D](https://github.com/urho3d/Urho3D) 的轻量级2D游戏引擎。

[English](README.md) | 中文

## 项目概述

TINA2D 是从 Urho3D 游戏引擎分支出来的专门针对2D游戏开发的版本。所有3D相关功能已被移除，专注于提供高效、简洁的2D游戏开发体验。

## 主要变更

- **移除3D功能**：删除了60多个3D相关类和整个3D渲染管线
- **保留2D核心**：完整保留了Urho2D、Physics2D、UI系统等2D功能
- **SDL3升级**：将底层SDL库从SDL2升级到SDL3，获得更好的性能和新特性
- **精简构建**：移除了AngelScript、IK、Navigation、3D Physics等依赖
- **优化示例**：保留并优化了2D相关示例，移除了3D示例

## 基于的开源项目

本项目基于 [Urho3D](https://github.com/urho3d/Urho3D) 游戏引擎，Urho3D是一个功能强大的跨平台2D和3D游戏引擎。

## 特性

- **专注2D**：纯2D游戏引擎，没有3D功能的开销
- **高性能**：基于SDL3的现代图形后端
- **丰富2D功能**：精灵渲染、粒子系统、瓦片地图、物理引擎
- **易于使用**：简化的API和丰富的示例代码
- **跨平台**：支持Windows、Linux、macOS等多个平台
- **轻量级**：相比完整的Urho3D，体积和依赖大幅减少

## 构建要求

- CMake 3.15+
- C++17兼容编译器
- SDL3库（已包含）

## 快速开始

```bash
# 克隆项目
git clone [你的仓库地址]
cd Tina2d

# 构建项目
mkdir build && cd build
cmake ..
cmake --build .
```

## 示例项目

TINA2D 包含多个2D游戏示例：

- **精灵渲染** - 2D精灵显示和动画
- **粒子系统** - 2D粒子效果
- **物理模拟** - 2D物理碰撞和约束
- **瓦片地图** - TMX格式地图支持
- **UI系统** - 完整的用户界面组件

## 技术架构

TINA2D 保留了Urho3D的核心架构，但专门为2D优化：

- **渲染器**：基于SDL3的2D渲染管线
- **场景系统**：节点-组件架构
- **资源管理**：高效的资源缓存系统
- **事件系统**：发布-订阅模式的事件处理
- **输入处理**：键盘、鼠标、触摸输入支持

## 许可证

TINA2D 基于 MIT 许可证发布。详情请参见 [LICENSE](licenses/urho3d/LICENSE)。

第三方库许可证：[licenses](licenses)

致谢：[licenses/urho3d/credits.md](licenses/urho3d/credits.md)

## 相关链接

### 原始项目
* [Urho3D 官方仓库](https://github.com/urho3d/Urho3D)
* [Urho3D 文档](https://urho3d-doxygen.github.io/latest/index.html)
* [Urho3D 社区论坛](https://github.com/urho3d-community/discussions/discussions)

### SDL3相关
* [SDL3 官方文档](https://wiki.libsdl.org/SDL3/FrontPage)
* [SDL3 迁移指南](https://github.com/libsdl-org/SDL/blob/main/docs/README-migration.md)

## 项目作者

**TINA2D** 基于 [Urho3D](https://github.com/urho3d/Urho3D) 项目。

Urho3D 的创始人是 [Lasse Öörni](https://github.com/cadaver)。同时感谢 [Yao Wei Tjong](https://github.com/weitjong)、[asterj](https://github.com/aster2013) 以及 [众多贡献者](https://github.com/urho3d/Urho3D/graphs/contributors) 的巨大贡献。

TINA2D 的2D专项优化和SDL3升级由项目维护团队完成。