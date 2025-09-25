// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"
#include "RmlUIRenderer.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Texture2D.h"
#include "../Resource/ResourceCache.h"
#include "../IO/Log.h"
#include <RmlUi/Core.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

namespace Urho3D
{

// 顶点着色器 - GLSL 330 格式
static const char* vertexShaderSource = R"(
#version 330
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_texcoord;

uniform mat4 u_transform;

out vec2 v_texcoord;
out vec4 v_color;

void main()
{
    gl_Position = u_transform * vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
}
)";

// 片段着色器 - GLSL 330 格式
static const char* fragmentShaderSource = R"(
#version 330
in vec2 v_texcoord;
in vec4 v_color;

uniform sampler2D s_texture;

out vec4 fragColor;

void main()
{
    vec4 texColor = texture(s_texture, v_texcoord);
    fragColor = texColor * v_color;
}
)";

RmlUIRenderer::RmlUIRenderer(Context* context)
    : Object(context)
    , viewId_(200)
    , nextTextureId_(1)
    , nextGeometryId_(1)
    , scissorEnabled_(false)
{
    graphics_ = GetSubsystem<Graphics>();
    cache_ = GetSubsystem<ResourceCache>();
}

RmlUIRenderer::~RmlUIRenderer()
{
    Shutdown();
}

bool RmlUIRenderer::Initialize()
{
    // 初始化顶点布局
    vertexLayout_
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    
    // 创建着色器程序
    program_ = GetShaderProgram();
    if (!bgfx::isValid(program_))
    {
        URHO3D_LOGERROR("Failed to create RmlUI shader program");
        return false;
    }
    
    // 初始化变换矩阵
    transform_ = Rml::Matrix4f::Identity();
    UpdateProjection();
    
    URHO3D_LOGINFO("RmlUI renderer initialized");
    return true;
}

void RmlUIRenderer::Shutdown()
{
    // 释放所有纹理
    for (auto& pair : textures_)
    {
        if (bgfx::isValid(pair.second))
            bgfx::destroy(pair.second);
    }
    textures_.Clear();
    
    // 释放所有编译的几何体
    for (auto& pair : compiledGeometries_)
    {
        const auto& geom = pair.second;
        if (bgfx::isValid(geom.vertexBuffer))
            bgfx::destroy(geom.vertexBuffer);
        if (bgfx::isValid(geom.indexBuffer))
            bgfx::destroy(geom.indexBuffer);
    }
    compiledGeometries_.Clear();
    
    // 释放着色器程序
    if (bgfx::isValid(program_))
    {
        bgfx::destroy(program_);
        program_ = BGFX_INVALID_HANDLE;
    }
}

void RmlUIRenderer::RenderGeometry(Rml::Vertex* vertices, int num_vertices,
                                   int* indices, int num_indices,
                                   Rml::TextureHandle texture,
                                   const Rml::Vector2f& translation)
{
    if (num_vertices == 0 || num_indices == 0)
        return;
    
    // 创建临时顶点缓冲区
    const bgfx::Memory* vbMem = bgfx::alloc(num_vertices * sizeof(Rml::Vertex));
    memcpy(vbMem->data, vertices, num_vertices * sizeof(Rml::Vertex));
    bgfx::VertexBufferHandle vb = bgfx::createVertexBuffer(vbMem, vertexLayout_);
    
    // 创建临时索引缓冲区
    const bgfx::Memory* ibMem = bgfx::alloc(num_indices * sizeof(uint32_t));
    uint32_t* indexData = (uint32_t*)ibMem->data;
    for (int i = 0; i < num_indices; ++i)
        indexData[i] = (uint32_t)indices[i];
    bgfx::IndexBufferHandle ib = bgfx::createIndexBuffer(ibMem, BGFX_BUFFER_INDEX32);
    
    // 设置变换矩阵
    float finalTransform[16];
    bx::mtxMul(finalTransform, (float*)&transform_, projection_);
    if (translation.x != 0.0f || translation.y != 0.0f)
    {
        float translationMtx[16];
        bx::mtxTranslate(translationMtx, translation.x, translation.y, 0.0f);
        bx::mtxMul(finalTransform, translationMtx, finalTransform);
    }
    
    // 设置渲染状态
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
    
    // 设置纹理
    bgfx::TextureHandle tex = BGFX_INVALID_HANDLE;
    if (texture)
    {
        auto it = textures_.Find(texture);
        if (it != textures_.End())
            tex = it->second;
    }
    
    if (!bgfx::isValid(tex))
    {
        // 使用白色纹理作为默认
        // TODO: 创建默认白色纹理
    }
    
    // 设置剪裁区域
    if (scissorEnabled_)
    {
        bgfx::setScissor(scissorRect_.left_, scissorRect_.top_,
                        scissorRect_.Width(), scissorRect_.Height());
    }
    
    // 提交绘制调用
    bgfx::setVertexBuffer(0, vb);
    bgfx::setIndexBuffer(ib);
    bgfx::setUniform(bgfx::createUniform("u_transform", bgfx::UniformType::Mat4), finalTransform);
    if (bgfx::isValid(tex))
        bgfx::setTexture(0, bgfx::createUniform("s_texture", bgfx::UniformType::Sampler), tex);
    bgfx::setState(state);
    bgfx::submit(viewId_, program_);
    
    // 清理临时缓冲区（bgfx 会在帧结束时自动销毁）
    bgfx::destroy(vb);
    bgfx::destroy(ib);
}

Rml::CompiledGeometryHandle RmlUIRenderer::CompileGeometry(Rml::Vertex* vertices, int num_vertices,
                                                          int* indices, int num_indices,
                                                          Rml::TextureHandle texture)
{
    if (num_vertices == 0 || num_indices == 0)
        return 0;
    
    CompiledGeometry geom;
    
    // 创建静态顶点缓冲区
    const bgfx::Memory* vbMem = bgfx::alloc(num_vertices * sizeof(Rml::Vertex));
    memcpy(vbMem->data, vertices, num_vertices * sizeof(Rml::Vertex));
    geom.vertexBuffer = bgfx::createVertexBuffer(vbMem, vertexLayout_);
    
    // 创建静态索引缓冲区
    const bgfx::Memory* ibMem = bgfx::alloc(num_indices * sizeof(uint32_t));
    uint32_t* indexData = (uint32_t*)ibMem->data;
    for (int i = 0; i < num_indices; ++i)
        indexData[i] = (uint32_t)indices[i];
    geom.indexBuffer = bgfx::createIndexBuffer(ibMem, BGFX_BUFFER_INDEX32);
    
    geom.numIndices = num_indices;
    
    // 保存纹理句柄
    if (texture)
    {
        auto it = textures_.Find(texture);
        if (it != textures_.End())
            geom.texture = it->second;
    }
    
    Rml::CompiledGeometryHandle handle = nextGeometryId_++;
    compiledGeometries_[handle] = geom;
    
    return handle;
}

void RmlUIRenderer::RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry,
                                          const Rml::Vector2f& translation)
{
    auto it = compiledGeometries_.Find(geometry);
    if (it == compiledGeometries_.End())
        return;
    
    const CompiledGeometry& geom = it->second;
    
    // 设置变换矩阵
    float finalTransform[16];
    bx::mtxMul(finalTransform, (float*)&transform_, projection_);
    if (translation.x != 0.0f || translation.y != 0.0f)
    {
        float translationMtx[16];
        bx::mtxTranslate(translationMtx, translation.x, translation.y, 0.0f);
        bx::mtxMul(finalTransform, translationMtx, finalTransform);
    }
    
    // 设置渲染状态
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
    
    // 设置剪裁区域
    if (scissorEnabled_)
    {
        bgfx::setScissor(scissorRect_.left_, scissorRect_.top_,
                        scissorRect_.Width(), scissorRect_.Height());
    }
    
    // 提交绘制调用
    bgfx::setVertexBuffer(0, geom.vertexBuffer);
    bgfx::setIndexBuffer(geom.indexBuffer);
    bgfx::setUniform(bgfx::createUniform("u_transform", bgfx::UniformType::Mat4), finalTransform);
    if (bgfx::isValid(geom.texture))
        bgfx::setTexture(0, bgfx::createUniform("s_texture", bgfx::UniformType::Sampler), geom.texture);
    bgfx::setState(state);
    bgfx::submit(viewId_, program_);
}

void RmlUIRenderer::ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry)
{
    auto it = compiledGeometries_.Find(geometry);
    if (it == compiledGeometries_.End())
        return;
    
    const CompiledGeometry& geom = it->second;
    if (bgfx::isValid(geom.vertexBuffer))
        bgfx::destroy(geom.vertexBuffer);
    if (bgfx::isValid(geom.indexBuffer))
        bgfx::destroy(geom.indexBuffer);
    
    compiledGeometries_.Erase(it);
}

void RmlUIRenderer::EnableScissorRegion(bool enable)
{
    scissorEnabled_ = enable;
}

void RmlUIRenderer::SetScissorRegion(int x, int y, int width, int height)
{
    scissorRect_ = IntRect(x, y, x + width, y + height);
}

bool RmlUIRenderer::LoadTexture(Rml::TextureHandle& texture_handle,
                               Rml::Vector2i& texture_dimensions,
                               const Rml::String& source)
{
    // 加载纹理资源
    SharedPtr<Texture2D> tex = cache_->GetResource<Texture2D>(String(source.c_str()));
    if (!tex)
    {
        URHO3D_LOGERRORF("Failed to load texture: %s", source.c_str());
        return false;
    }
    
    texture_dimensions.x = tex->GetWidth();
    texture_dimensions.y = tex->GetHeight();
    
    // 获取 bgfx 纹理句柄
    bgfx::TextureHandle bgfxTex = BGFX_INVALID_HANDLE;
    // TODO: 从 Texture2D 获取 bgfx 纹理句柄
    // bgfxTex = tex->GetBgfxHandle();
    
    texture_handle = nextTextureId_++;
    textures_[texture_handle] = bgfxTex;
    
    return true;
}

bool RmlUIRenderer::GenerateTexture(Rml::TextureHandle& texture_handle,
                                   const Rml::byte* source,
                                   const Rml::Vector2i& source_dimensions)
{
    // 创建纹理内存
    const bgfx::Memory* mem = bgfx::alloc(source_dimensions.x * source_dimensions.y * 4);
    memcpy(mem->data, source, mem->size);
    
    // 创建纹理
    bgfx::TextureHandle tex = bgfx::createTexture2D(
        source_dimensions.x, source_dimensions.y,
        false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
        mem
    );
    
    if (!bgfx::isValid(tex))
    {
        URHO3D_LOGERROR("Failed to generate texture");
        return false;
    }
    
    texture_handle = nextTextureId_++;
    textures_[texture_handle] = tex;
    
    return true;
}

void RmlUIRenderer::ReleaseTexture(Rml::TextureHandle texture)
{
    auto it = textures_.Find(texture);
    if (it == textures_.End())
        return;
    
    if (bgfx::isValid(it->second))
        bgfx::destroy(it->second);
    
    textures_.Erase(it);
}

void RmlUIRenderer::SetTransform(const Rml::Matrix4f* transform)
{
    if (transform)
        transform_ = *transform;
    else
        transform_ = Rml::Matrix4f::Identity();
}

void RmlUIRenderer::UpdateProjection()
{
    if (!graphics_)
        return;
    
    float width = (float)graphics_->GetWidth();
    float height = (float)graphics_->GetHeight();
    
    // 创建正交投影矩阵
    bx::mtxOrtho(projection_, 0.0f, width, height, 0.0f, -1.0f, 1.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
}

bgfx::ProgramHandle RmlUIRenderer::GetShaderProgram()
{
    // TODO: 使用 bgfx shaderc 编译着色器
    // 这里暂时返回无效句柄，需要实现着色器编译
    return BGFX_INVALID_HANDLE;
}

} // namespace Urho3D