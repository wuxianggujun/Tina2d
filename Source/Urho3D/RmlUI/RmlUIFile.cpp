// Copyright (c) 2024 Tina2D project
// SPDX-License-Identifier: MIT

#include "../Precompiled.h"
#include "RmlUIFile.h"
#include "../Resource/ResourceCache.h"
#include "../IO/File.h"
#include "../IO/Log.h"

namespace Urho3D
{

RmlUIFile::RmlUIFile(Context* context)
    : Object(context)
    , basePath_("Data/UI/")
    , hotReload_(false)
    , nextFileId_(1)
{
    cache_ = GetSubsystem<ResourceCache>();
}

RmlUIFile::~RmlUIFile()
{
    // 关闭所有打开的文件
    openFiles_.Clear();
}

Rml::FileHandle RmlUIFile::Open(const Rml::String& path)
{
    if (!cache_)
    {
        URHO3D_LOGERROR("ResourceCache not available");
        return 0;
    }
    
    // 构建完整路径
    String fullPath = basePath_ + String(path.c_str());
    
    // 尝试打开文件
    SharedPtr<File> file = cache_->GetFile(fullPath);
    if (!file || !file->IsOpen())
    {
        URHO3D_LOGERRORF("Failed to open file: %s", fullPath.CString());
        return 0;
    }
    
    // 读取文件内容
    FileInfo info;
    info.data.Resize(file->GetSize());
    info.position = 0;
    
    if (file->Read(&info.data[0], info.data.Size()) != info.data.Size())
    {
        URHO3D_LOGERRORF("Failed to read file: %s", fullPath.CString());
        return 0;
    }
    
    // 存储文件信息
    Rml::FileHandle handle = nextFileId_++;
    openFiles_[handle] = info;
    
    URHO3D_LOGDEBUGF("Opened RmlUI file: %s (handle: %d)", fullPath.CString(), handle);
    return handle;
}

void RmlUIFile::Close(Rml::FileHandle file)
{
    auto it = openFiles_.Find(file);
    if (it != openFiles_.End())
    {
        openFiles_.Erase(it);
        URHO3D_LOGDEBUGF("Closed RmlUI file handle: %d", file);
    }
}

size_t RmlUIFile::Read(void* buffer, size_t size, Rml::FileHandle file)
{
    auto it = openFiles_.Find(file);
    if (it == openFiles_.End())
        return 0;
    
    FileInfo& info = it->second;
    
    // 计算可读取的字节数
    size_t bytesAvailable = info.data.Size() - info.position;
    size_t bytesToRead = Min(size, bytesAvailable);
    
    if (bytesToRead > 0)
    {
        memcpy(buffer, &info.data[info.position], bytesToRead);
        info.position += bytesToRead;
    }
    
    return bytesToRead;
}

bool RmlUIFile::Seek(Rml::FileHandle file, long offset, int origin)
{
    auto it = openFiles_.Find(file);
    if (it == openFiles_.End())
        return false;
    
    FileInfo& info = it->second;
    size_t newPosition = 0;
    
    switch (origin)
    {
    case SEEK_SET:
        newPosition = offset;
        break;
    case SEEK_CUR:
        newPosition = info.position + offset;
        break;
    case SEEK_END:
        newPosition = info.data.Size() + offset;
        break;
    default:
        return false;
    }
    
    if (newPosition > info.data.Size())
        return false;
    
    info.position = newPosition;
    return true;
}

size_t RmlUIFile::Tell(Rml::FileHandle file)
{
    auto it = openFiles_.Find(file);
    if (it == openFiles_.End())
        return 0;
    
    return it->second.position;
}

size_t RmlUIFile::Length(Rml::FileHandle file)
{
    auto it = openFiles_.Find(file);
    if (it == openFiles_.End())
        return 0;
    
    return it->second.data.Size();
}

} // namespace Urho3D