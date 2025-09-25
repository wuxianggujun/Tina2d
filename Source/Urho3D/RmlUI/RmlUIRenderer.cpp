// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"
#include "RmlUIRenderer.h"
#include "../Graphics/Graphics.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/Image.h"
#include "../IO/Log.h"
#include <RmlUi/Core.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

namespace Urho3D
{

RmlUIRenderer::RmlUIRenderer(Context* context)
    : Object(context)
{
    graphics_ = GetSubsystem<Graphics>();
    cache_ = GetSubsystem<ResourceCache>();
    
    // 初始化顶点布局
    vertexLayout_
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
}

RmlUIRenderer::~RmlUIRenderer()
{
    Shutdown();
}

bool RmlUIRenderer::Initialize()
{
    // 设置初始投影矩阵
    UpdateProjection(graphics_->GetWidth(), graphics_->GetHeight());
    
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
        if (bgfx::isValid(pair.second.vertexBuffer))
            bgfx::destroy(pair.second.vertexBuffer);
        if (bgfx::isValid(pair.second.indexBuffer))
            bgfx::destroy(pair.second.indexBuffer);
    }
    compiledGeometries_.Clear();
    
    // 释放着色器程序
    if (bgfx::isValid(program_))
    {
        bgfx::destroy(program_);
        program_ = BGFX_INVALID_HANDLE;
    }
}

Rml::CompiledGeometryHandle RmlUIRenderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                           Rml::Span<const int> indices)
{
    if (vertices.empty() || indices.empty())
        return 0;
    
    // 创建顶点缓冲区
    const bgfx::Memory* vertexMem = bgfx::alloc((uint32_t)(vertices.size() * sizeof(Rml::Vertex)));
    memcpy(vertexMem->data, vertices.data(), vertices.size() * sizeof(Rml::Vertex));
    bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(vertexMem, vertexLayout_);
    
    // 创建索引缓冲区
    const bgfx::Memory* indexMem = bgfx::alloc((uint32_t)(indices.size() * sizeof(int)));
    memcpy(indexMem->data, indices.data(), indices.size() * sizeof(int));
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(indexMem, BGFX_BUFFER_INDEX32);
    
    // 保存编译的几何体
    CompiledGeometry geom;
    geom.vertexBuffer = vbh;
    geom.indexBuffer = ibh;
    geom.numIndices = (int)indices.size();
    
    Rml::CompiledGeometryHandle handle = nextGeometryId_++;
    compiledGeometries_[handle] = geom;
    
    return handle;
}

void RmlUIRenderer::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                   Rml::Vector2f translation,
                                   Rml::TextureHandle texture)
{
    if (geometry == 0)
        return;
    
    auto it = compiledGeometries_.Find(geometry);
    if (it == compiledGeometries_.End())
        return;
    
    const CompiledGeometry& geom = it->second;
    
    // 设置渲染状态
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;
    state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
    
    // 应用变换
    float transform[16];
    bx::mtxIdentity(transform);
    if (transform_.data())
    {
        memcpy(transform, transform_.data(), sizeof(float) * 16);
    }
    transform[12] += translation.x;
    transform[13] += translation.y;
    
    bgfx::setTransform(transform);
    bgfx::setVertexBuffer(0, geom.vertexBuffer);
    bgfx::setIndexBuffer(geom.indexBuffer, 0, geom.numIndices);
    
    // 设置纹理
    if (texture)
    {
        auto texIt = textures_.Find(texture);
        if (texIt != textures_.End())
        {
            bgfx::UniformHandle texUniform = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
            bgfx::setTexture(0, texUniform, texIt->second);
        }
    }
    
    // 设置着色器程序
    bgfx::ProgramHandle program = GetShaderProgram();
    if (bgfx::isValid(program))
    {
        bgfx::setState(state);
        bgfx::submit(viewId_, program);
    }
}

void RmlUIRenderer::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    auto it = compiledGeometries_.Find(geometry);
    if (it != compiledGeometries_.End())
    {
        if (bgfx::isValid(it->second.vertexBuffer))
            bgfx::destroy(it->second.vertexBuffer);
        if (bgfx::isValid(it->second.indexBuffer))
            bgfx::destroy(it->second.indexBuffer);
        compiledGeometries_.Erase(it);
    }
}

Rml::TextureHandle RmlUIRenderer::LoadTexture(Rml::Vector2i& texture_dimensions,
                                              const Rml::String& source)
{
    // 从资源缓存加载图像
    SharedPtr<Image> image(cache_->GetResource<Image>(source.c_str()));
    if (!image)
    {
        URHO3D_LOGERROR("Failed to load texture: " + String(source.c_str()));
        return 0;
    }
    
    // 设置纹理尺寸
    texture_dimensions.x = image->GetWidth();
    texture_dimensions.y = image->GetHeight();
    
    // 创建bgfx纹理
    const bgfx::Memory* mem = bgfx::alloc(image->GetWidth() * image->GetHeight() * 4);
    memcpy(mem->data, image->GetData(), mem->size);
    
    bgfx::TextureHandle texHandle = bgfx::createTexture2D(
        image->GetWidth(), image->GetHeight(), false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
        mem
    );
    
    if (!bgfx::isValid(texHandle))
    {
        URHO3D_LOGERROR("Failed to create bgfx texture");
        return 0;
    }
    
    // 保存纹理句柄
    Rml::TextureHandle handle = nextTextureId_++;
    textures_[handle] = texHandle;
    
    return handle;
}

Rml::TextureHandle RmlUIRenderer::GenerateTexture(Rml::Span<const Rml::byte> source,
                                                  Rml::Vector2i source_dimensions)
{
    if (source.empty() || source_dimensions.x <= 0 || source_dimensions.y <= 0)
        return 0;
    
    // 创建bgfx纹理
    const bgfx::Memory* mem = bgfx::alloc((uint32_t)source.size());
    memcpy(mem->data, source.data(), source.size());
    
    bgfx::TextureHandle texHandle = bgfx::createTexture2D(
        source_dimensions.x, source_dimensions.y, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
        mem
    );
    
    if (!bgfx::isValid(texHandle))
    {
        URHO3D_LOGERROR("Failed to generate bgfx texture");
        return 0;
    }
    
    // 保存纹理句柄
    Rml::TextureHandle handle = nextTextureId_++;
    textures_[handle] = texHandle;
    
    return handle;
}

void RmlUIRenderer::ReleaseTexture(Rml::TextureHandle texture)
{
    auto it = textures_.Find(texture);
    if (it != textures_.End())
    {
        if (bgfx::isValid(it->second))
            bgfx::destroy(it->second);
        textures_.Erase(it);
    }
}

void RmlUIRenderer::EnableScissorRegion(bool enable)
{
    scissorEnabled_ = enable;
    if (!enable)
    {
        bgfx::setScissor();
    }
}

void RmlUIRenderer::SetScissorRegion(Rml::Rectanglei region)
{
    if (scissorEnabled_)
    {
        scissorRect_ = IntRect(region.Left(), region.Top(), 
                               region.Right(), region.Bottom());
        bgfx::setScissor(scissorRect_.left_, scissorRect_.top_, 
                        scissorRect_.Width(), scissorRect_.Height());
    }
}

void RmlUIRenderer::SetTransform(const Rml::Matrix4f* transform)
{
    if (transform)
    {
        transform_ = *transform;
    }
    else
    {
        // 使用单位矩阵
        transform_ = Rml::Matrix4f::Identity();
    }
}

void RmlUIRenderer::UpdateProjection(int width, int height)
{
    // 创建正交投影矩阵
    bx::mtxOrtho(projection_, 0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
    
    // 设置视口
    bgfx::setViewRect(viewId_, 0, 0, width, height);
    bgfx::setViewTransform(viewId_, nullptr, projection_);
    bgfx::setViewClear(viewId_, BGFX_CLEAR_NONE);
}

bgfx::ProgramHandle RmlUIRenderer::GetShaderProgram()
{
    if (!bgfx::isValid(program_))
    {
        // TODO: 实现着色器编译
        // 这里需要使用shaderc编译UI着色器
        // 或者从预编译的着色器二进制文件加载
        URHO3D_LOGWARNING("UI shader program not yet implemented");
    }
    
    return program_;
}

} // namespace Urho3D