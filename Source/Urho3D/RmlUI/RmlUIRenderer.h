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

    // Rml::RenderInterface implementation
    /// Called by RmlUi when it wants to render geometry that it does not wish to optimise.
    void RenderGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices,
                       Rml::TextureHandle texture, const Rml::Vector2f& translation) override;

    /// Called by RmlUi when it wants to compile geometry it believes will be static for the forseeable future.
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::TextureHandle texture) override;
    /// Called by RmlUi when it wants to render compiled geometry.
    void RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry, const Rml::Vector2f& translation) override;
    /// Called by RmlUi when it wants to release compiled geometry.
    void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry) override;

    /// Called by RmlUi when it wants to enable or disable scissoring to clip content.
    void EnableScissorRegion(bool enable) override;
    /// Called by RmlUi when it wants to change the scissor region.
    void SetScissorRegion(int x, int y, int width, int height) override;

    /// Called by RmlUi when a texture is required by the library.
    bool LoadTexture(Rml::TextureHandle& texture_handle, Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    /// Called by RmlUi when a texture is required to be built from an internally-generated sequence of pixels.
    bool GenerateTexture(Rml::TextureHandle& texture_handle, const Rml::byte* source, const Rml::Vector2i& source_dimensions) override;
    /// Called by RmlUi when a loaded texture is no longer required.
    void ReleaseTexture(Rml::TextureHandle texture) override;

    /// Called by RmlUi when it wants to set the current transform matrix to a new matrix.
    void SetTransform(const Rml::Matrix4f* transform) override;

    /// Set UI view ID for bgfx (default: 200).
    void SetViewId(bgfx::ViewId id) { viewId_ = id; }
    /// Get UI view ID.
    bgfx::ViewId GetViewId() const { return viewId_; }

private:
    /// Compiled geometry structure.
    struct CompiledGeometry
    {
        bgfx::VertexBufferHandle vertexBuffer;
        bgfx::IndexBufferHandle indexBuffer;
        bgfx::TextureHandle texture;
        int numIndices;
    };

    /// Update projection matrix.
    void UpdateProjection();
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