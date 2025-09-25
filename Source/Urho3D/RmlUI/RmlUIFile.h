// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/Object.h"
#include <RmlUi/Core/FileInterface.h>

namespace Urho3D
{

class Context;
class ResourceCache;

/// RmlUI file interface implementation using Urho3D ResourceCache.
class RmlUIFile : public Object, public Rml::FileInterface
{
    URHO3D_OBJECT(RmlUIFile, Object);

public:
    /// Construct.
    explicit RmlUIFile(Context* context);
    /// Destruct.
    ~RmlUIFile() override;

    // Rml::FileInterface implementation
    /// Opens a file.
    Rml::FileHandle Open(const Rml::String& path) override;
    /// Closes a previously opened file.
    void Close(Rml::FileHandle file) override;
    /// Reads data from a previously opened file.
    size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
    /// Seeks to a point in a previously opened file.
    bool Seek(Rml::FileHandle file, long offset, int origin) override;
    /// Returns the current position of the file pointer.
    size_t Tell(Rml::FileHandle file) override;
    /// Returns the length of the file.
    size_t Length(Rml::FileHandle file) override;

    /// Set base path for UI resources.
    void SetBasePath(const String& path) { basePath_ = path; }
    /// Get base path.
    const String& GetBasePath() const { return basePath_; }

    /// Enable hot reload for RML/CSS files.
    void SetHotReloadEnabled(bool enable) { hotReload_ = enable; }
    /// Check if hot reload is enabled.
    bool IsHotReloadEnabled() const { return hotReload_; }

private:
    /// Resource cache.
    WeakPtr<ResourceCache> cache_;
    /// Base path for UI resources.
    String basePath_ = "Data/UI/";
    /// Hot reload enabled flag.
    bool hotReload_ = false;
    
    /// File handle structure.
    struct FileInfo
    {
        Vector<unsigned char> data;
        size_t position = 0;
    };
    
    /// Open files map.
    HashMap<Rml::FileHandle, FileInfo> openFiles_;
    /// Next file handle ID.
    Rml::FileHandle nextFileId_ = 1;
};

} // namespace Urho3D