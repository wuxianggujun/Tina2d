// Copyright (c) 2008-2023 the Urho3D project
// License: MIT

#include "../Precompiled.h"

#include "../Audio/Audio.h"
#include "../Audio/Sound.h"
#include "../Audio/SoundListener.h"
#include "../Audio/SoundSource.h" // 使用 SoundSource 成员与静态注册需要完整类型
#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../Core/ProcessUtils.h"
#include "../Core/Profiler.h"
#include "../IO/Log.h"

#include <SDL3/SDL.h>

#include "../DebugNew.h"

namespace Urho3D
{

const char* AUDIO_CATEGORY = "Audio";

static const i32 MIN_BUFFERLENGTH = 20;
static const i32 MIN_MIXRATE = 11025;
static const i32 MAX_MIXRATE = 48000;
static const StringHash SOUND_MASTER_HASH("Master");

static void SDLAudioCallback(void* userdata, Uint8* stream, i32 len);
static void SDLAudioStreamGetCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/);

Audio::Audio(Context* context) :
    Object(context)
{
    context_->RequireSDL(SDL_INIT_AUDIO);

    // Set the master to the default value
    masterGain_[SOUND_MASTER_HASH] = 1.0f;

    // Register Audio library object factories
    RegisterAudioLibrary(context_);

    SubscribeToEvent(E_RENDERUPDATE, URHO3D_HANDLER(Audio, HandleRenderUpdate));
}

Audio::~Audio()
{
    Release();
    context_->ReleaseSDL();
}

bool Audio::SetMode(i32 bufferLengthMSec, i32 mixRate, bool stereo, bool interpolation)
{
    Release();

    bufferLengthMSec = Max(bufferLengthMSec, MIN_BUFFERLENGTH);
    mixRate = Clamp(mixRate, MIN_MIXRATE, MAX_MIXRATE);

    SDL_AudioSpec desired{};
    desired.freq = mixRate;
    desired.format = SDL_AUDIO_S16;
    desired.channels = stereo ? 2 : 1;

    // 打开默认播放设备（SDL3 新 API）
    deviceID_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (!deviceID_)
    {
        URHO3D_LOGERROR("Could not initialize audio output");
        return false;
    }

    // 在设备上创建并绑定音频流，使用拉取回调补充数据
    audioStream_ = SDL_OpenAudioDeviceStream(deviceID_, &desired, SDLAudioStreamGetCallback, this);
    if (!audioStream_)
    {
        URHO3D_LOGERROR("Could not create audio stream");
        SDL_CloseAudioDevice(deviceID_);
        deviceID_ = 0;
        return false;
    }

    stereo_ = stereo;
    sampleSize_ = (u32)(stereo_ ? sizeof(i32) : sizeof(i16));
    // 选择保守的片段大小用于内部混音（不再依赖设备 samples）
    fragmentSize_ = NextPowerOfTwo((u32)mixRate >> 6u);
    mixRate_ = mixRate;
    interpolation_ = interpolation;
    clipBuffer_ = new i32[stereo ? fragmentSize_ << 1u : fragmentSize_];

    URHO3D_LOGINFO("Set audio mode " + String(mixRate_) + " Hz " + (stereo_ ? "stereo" : "mono") + (interpolation_ ? " interpolated" : ""));

    return Play();
}

void Audio::Update(float timeStep)
{
    if (!playing_)
        return;

    UpdateInternal(timeStep);
}

bool Audio::Play()
{
    if (playing_)
        return true;

    if (!deviceID_)
    {
        URHO3D_LOGERROR("No audio mode set, can not start playback");
        return false;
    }

    // 设备由流驱动，恢复流绑定的输出
    if (audioStream_)
        SDL_ResumeAudioStreamDevice((SDL_AudioStream*)audioStream_);

    // Update sound sources before resuming playback to make sure 3D positions are up to date
    UpdateInternal(0.0f);

    playing_ = true;
    return true;
}

void Audio::Stop()
{
    playing_ = false;
}

void Audio::SetMasterGain(const String& type, float gain)
{
    masterGain_[type] = Clamp(gain, 0.0f, 1.0f);

    for (Vector<SoundSource*>::Iterator i = soundSources_.Begin(); i != soundSources_.End(); ++i)
        (*i)->UpdateMasterGain();
}

void Audio::PauseSoundType(const String& type)
{
    MutexLock lock(audioMutex_);
    pausedSoundTypes_.Insert(type);
}

void Audio::ResumeSoundType(const String& type)
{
    MutexLock lock(audioMutex_);
    pausedSoundTypes_.Erase(type);
    // Update sound sources before resuming playback to make sure 3D positions are up to date
    // Done under mutex to ensure no mixing happens before we are ready
    UpdateInternal(0.0f);
}

void Audio::ResumeAll()
{
    MutexLock lock(audioMutex_);
    pausedSoundTypes_.Clear();
    UpdateInternal(0.0f);
}

void Audio::SetListener(SoundListener* listener)
{
    listener_ = listener;
}

void Audio::StopSound(Sound* sound)
{
    for (Vector<SoundSource*>::Iterator i = soundSources_.Begin(); i != soundSources_.End(); ++i)
    {
        if ((*i)->GetSound() == sound)
            (*i)->Stop();
    }
}

float Audio::GetMasterGain(const String& type) const
{
    // By definition previously unknown types return full volume
    HashMap<StringHash, Variant>::ConstIterator findIt = masterGain_.Find(type);
    if (findIt == masterGain_.End())
        return 1.0f;

    return findIt->second_.GetFloat();
}

bool Audio::IsSoundTypePaused(const String& type) const
{
    return pausedSoundTypes_.Contains(type);
}

SoundListener* Audio::GetListener() const
{
    return listener_;
}

void Audio::AddSoundSource(SoundSource* soundSource)
{
    MutexLock lock(audioMutex_);
    soundSources_.Push(soundSource);
}

void Audio::RemoveSoundSource(SoundSource* soundSource)
{
    Vector<SoundSource*>::Iterator i = soundSources_.Find(soundSource);
    if (i != soundSources_.End())
    {
        MutexLock lock(audioMutex_);
        soundSources_.Erase(i);
    }
}

float Audio::GetSoundSourceMasterGain(StringHash typeHash) const
{
    HashMap<StringHash, Variant>::ConstIterator masterIt = masterGain_.Find(SOUND_MASTER_HASH);

    if (!typeHash)
        return masterIt->second_.GetFloat();

    HashMap<StringHash, Variant>::ConstIterator typeIt = masterGain_.Find(typeHash);

    if (typeIt == masterGain_.End() || typeIt == masterIt)
        return masterIt->second_.GetFloat();

    return masterIt->second_.GetFloat() * typeIt->second_.GetFloat();
}

void SDLAudioCallback(void* userdata, Uint8* stream, i32 len)
{
    auto* audio = static_cast<Audio*>(userdata);
    {
        MutexLock Lock(audio->GetMutex());
        audio->MixOutput(stream, len / audio->GetSampleSize());
    }
}

// SDL3 音频拉取回调：当设备需要更多数据时调用，在此填充并推入音频流
void SDLAudioStreamGetCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/)
{
    auto* audio = static_cast<Audio*>(userdata);
    if (additional_amount <= 0)
        return;

    MutexLock Lock(audio->GetMutex());
    const int samples = additional_amount / (int)audio->GetSampleSize();
    if (samples <= 0)
        return;

    // 生成所需的 PCM 数据并推入流
    SharedArrayPtr<u8> tmp(new u8[(size_t)additional_amount]);
    audio->MixOutput(tmp.Get(), (u32)samples);
    SDL_PutAudioStreamData(stream, tmp.Get(), additional_amount);
}

void Audio::MixOutput(void* dest, u32 samples)
{
    if (!playing_ || !clipBuffer_)
    {
        memset(dest, 0, samples * (size_t)sampleSize_);
        return;
    }

    while (samples)
    {
        // If sample count exceeds the fragment (clip buffer) size, split the work
        u32 workSamples = Min(samples, fragmentSize_);
        u32 clipSamples = workSamples;
        if (stereo_)
            clipSamples <<= 1;

        // Clear clip buffer
        i32* clipPtr = clipBuffer_.Get();
        memset(clipPtr, 0, clipSamples * sizeof(i32));

        // Mix samples to clip buffer
        for (Vector<SoundSource*>::Iterator i = soundSources_.Begin(); i != soundSources_.End(); ++i)
        {
            SoundSource* source = *i;

            // Check for pause if necessary
            if (!pausedSoundTypes_.Empty())
            {
                if (pausedSoundTypes_.Contains(source->GetSoundType()))
                    continue;
            }

            source->Mix(clipPtr, workSamples, mixRate_, stereo_, interpolation_);
        }
        // Copy output from clip buffer to destination
        auto* destPtr = (short*)dest;
        while (clipSamples--)
            *destPtr++ = (short)Clamp(*clipPtr++, -32768, 32767);
        samples -= workSamples;
        ((u8*&)dest) += sampleSize_ * workSamples;
    }
}

void Audio::HandleRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace RenderUpdate;

    Update(eventData[P_TIMESTEP].GetFloat());
}

void Audio::Release()
{
    Stop();

    if (audioStream_)
    {
        SDL_DestroyAudioStream((SDL_AudioStream*)audioStream_);
        audioStream_ = nullptr;
    }
    if (deviceID_)
    {
        SDL_CloseAudioDevice(deviceID_);
        deviceID_ = 0;
    }
    clipBuffer_.Reset();
}

void Audio::UpdateInternal(float timeStep)
{
    URHO3D_PROFILE(UpdateAudio);

    // Update in reverse order, because sound sources might remove themselves
    for (i32 i = soundSources_.Size() - 1; i >= 0; --i)
    {
        SoundSource* source = soundSources_[i];

        // Check for pause if necessary; do not update paused sound sources
        if (!pausedSoundTypes_.Empty())
        {
            if (pausedSoundTypes_.Contains(source->GetSoundType()))
                continue;
        }

        source->Update(timeStep);
    }
}

void RegisterAudioLibrary(Context* context)
{
    Sound::RegisterObject(context);
    SoundSource::RegisterObject(context);
    SoundListener::RegisterObject(context);
}

}
