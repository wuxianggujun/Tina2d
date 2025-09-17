// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#pragma once

#include "../IO/File.h"
#include "../IO/Serializer.h"
#include "../IO/Deserializer.h"

#include <SDL3/SDL_iostream.h>

namespace Urho3D
{

/// 使用 SDL3 IOStream 接口将 Urho3D 的 Serializer/Deserializer 适配为 SDL_IOStream。
template <class T> class RWOpsWrapper
{
public:
    /// Construct with object reference.
    explicit RWOpsWrapper(T& object) : object_(object)
    {
        SDL_INIT_INTERFACE(&iface_);
        iface_.size = &Size;
        iface_.seek = &Seek;
        iface_.read = &Read;
        iface_.write = &Write;
        iface_.flush = &Flush;
        iface_.close = &Close;
        stream_ = SDL_OpenIO(&iface_, this);
    }

    /// 返回 SDL_IOStream 指针（不转移所有权）。
    SDL_IOStream* GetIOStream() { return stream_; }

private:
    /// 返回数据总大小（仅对 Deserializer 有意义）。
    static Sint64 Size(void* userdata)
    {
        auto* self = static_cast<RWOpsWrapper<T>*>(userdata);
        if (auto* des = dynamic_cast<Deserializer*>(&self->object_))
            return (Sint64)des->GetSize();
        return -1;
    }

    /// 相对/绝对定位（仅对 Deserializer 有意义）。
    static Sint64 Seek(void* userdata, Sint64 offset, SDL_IOWhence whence)
    {
        auto* self = static_cast<RWOpsWrapper<T>*>(userdata);
        auto* des = dynamic_cast<Deserializer*>(&self->object_);
        if (!des)
            return -1;

        switch (whence)
        {
        case SDL_IO_SEEK_SET: des->Seek((i64)offset); break;
        case SDL_IO_SEEK_CUR: des->Seek((i64)(des->GetPosition() + offset)); break;
        case SDL_IO_SEEK_END: des->Seek((i64)(des->GetSize() + offset)); break;
        default: return -1;
        }
        return (Sint64)des->GetPosition();
    }

    /// 读回调（仅对 Deserializer 有意义）。
    static size_t Read(void* userdata, void* ptr, size_t size, SDL_IOStatus* status)
    {
        auto* self = static_cast<RWOpsWrapper<T>*>(userdata);
        if (auto* des = dynamic_cast<Deserializer*>(&self->object_))
        {
            const i32 readBytes = des->Read(ptr, (i32)size);
            if (status && (size_t)readBytes < size)
                *status = des->IsEof() ? SDL_IO_STATUS_EOF : SDL_IO_STATUS_ERROR;
            return (size_t)readBytes;
        }
        if (status) *status = SDL_IO_STATUS_WRITEONLY;
        return 0;
    }

    /// 写回调（仅对 Serializer 有意义）。
    static size_t Write(void* userdata, const void* ptr, size_t size, SDL_IOStatus* status)
    {
        auto* self = static_cast<RWOpsWrapper<T>*>(userdata);
        if (auto* ser = dynamic_cast<Serializer*>(&self->object_))
        {
            const i32 written = ser->Write(ptr, (i32)size);
            if (status && (size_t)written < size)
                *status = SDL_IO_STATUS_ERROR;
            return (size_t)written;
        }
        if (status) *status = SDL_IO_STATUS_READONLY;
        return 0;
    }

    /// Flush 回调（File 时可触发 Flush，其它忽略）。
    static bool Flush(void* userdata, SDL_IOStatus* status)
    {
        auto* self = static_cast<RWOpsWrapper<T>*>(userdata);
        if (auto* file = dynamic_cast<File*>(&self->object_))
        {
            file->Flush();
            return true;
        }
        // 无需 flush
        if (status) *status = SDL_IO_STATUS_READY;
        return true;
    }

    /// 关闭回调（File 时关闭文件，其它忽略）。
    static bool Close(void* userdata)
    {
        auto* self = static_cast<RWOpsWrapper<T>*>(userdata);
        if (auto* file = dynamic_cast<File*>(&self->object_))
        {
            file->Close();
        }
        return true;
    }

    T& object_;
    SDL_IOStreamInterface iface_{};
    SDL_IOStream* stream_{};
};

}
