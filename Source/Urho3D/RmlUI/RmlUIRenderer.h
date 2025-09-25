// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/Object.h"
#include <RmlUi/Core/RenderInterface.h>
#include <bgfx/bgfx.h>

namespace Urho3D
{

class Context;
class Graphics;
class ResourceCache;
class Texture2D;

/// RmlUI render interface implementation using bgfx.
class RmlUIRenderer : public Object, public Rml::RenderInterface
{
    URHO3D_OBJECT(RmlUIRenderer, Object);

public:
    /// Construct.
    explicit RmlUIRenderer(Context* context);
    /// Destruct.
    ~RmlUIRenderer() override;

    /// Initialize the renderer.
    bool Initialize();
    /// Shutdown the renderer.
    void Shutdown();

    // RenderInterface implementation - Required functions
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                       Rml::Vector2f translation,
                       Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    
    // Texture handling
    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions,
                                   const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                       Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    
    // Scissoring
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    
    // Transform handling
    void SetTransform(const Rml::Matrix4f* transform) override;

    /// Set UI view ID for bgfx (default: 200).
    void SetViewId(bgfx::ViewId id) { viewId_ = id; }
    /// Get UI view ID.
    bgfx::ViewId GetViewId() const { return viewId_; }
    
    /// Get the context for rendering
    Context* GetContext() const { return context_; }
    
    /// Update projection matrix
    void UpdateProjection(int width, int height);

private:
    /// Compiled geometry structure.
    struct CompiledGeometry
    {
        bgfx::VertexBufferHandle vertexBuffer;
        bgfx::IndexBufferHandle indexBuffer;
        bgfx::TextureHandle texture;
        int numIndices;
    };

    /// Get or create shader program for UI rendering.
    bgfx::ProgramHandle GetShaderProgram();

    /// Graphics subsystem.
    WeakPtr<Graphics> graphics_;
    /// Resource cache.
    WeakPtr<ResourceCache> cache_;
    
    /// bgfx view ID for UI rendering.
    bgfx::ViewId viewId_ = 200;
    /// Shader program for UI.
    bgfx::ProgramHandle program_ = BGFX_INVALID_HANDLE;
    /// Vertex layout for UI vertices.
    bgfx::VertexLayout vertexLayout_;
    
    /// Transform matrix.
    Rml::Matrix4f transform_;
    /// Projection matrix.
    float projection_[16];
    
    /// Scissor enabled flag.
    bool scissorEnabled_ = false;
    /// Scissor rectangle.
    IntRect scissorRect_;
    
    /// Texture handle map.
    HashMap<Rml::TextureHandle, bgfx::TextureHandle> textures_;
    /// Compiled geometry map.
    HashMap<Rml::CompiledGeometryHandle, CompiledGeometry> compiledGeometries_;
    /// Next texture handle ID.
    Rml::TextureHandle nextTextureId_ = 1;
    /// Next compiled geometry handle ID.
    Rml::CompiledGeometryHandle nextGeometryId_ = 1;
};

} // namespace Urho3D