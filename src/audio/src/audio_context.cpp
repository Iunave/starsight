#include "audio_context.hpp"
#include "core/log.hpp"
#include "core/assertion.hpp"
#include "core/utility_functions.hpp"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"
#include "core/filesystem.hpp"
#include <cstdlib>
#include <utility>
#include <vector>

struct IPLResultChecker
{
    IPLResultChecker& operator=(IPLerror Result_)
    {
        VERIFY(Result_ == IPL_STATUS_SUCCESS);
        Result = Result_;
        return *this;
    }

    IPLerror Result = IPLerror::IPL_STATUS_SUCCESS;
};

inline static thread_local constinit IPLResultChecker iplCheckResult{};

static std::vector<const char*> BuildVectorFromNullList(const char* List)
{
    std::vector<const char*> Vector{};
    const char* it = List;

    while(*it != 0)
    {
        Vector.emplace_back(it);

        while(*it != 0)
        {
            ++it;
        }
        ++it;
    }
    return Vector;
}

static void SteamAudioLogCallback(IPLLogLevel level, const char* message)
{
    switch(level)
    {
        case IPL_LOGLEVEL_INFO:
            LOG_INFO("{}", message);
            break;
        case IPL_LOGLEVEL_WARNING:
            LOG_WARNING("{}", message);
            break;
        case IPL_LOGLEVEL_ERROR:
            LOG_ERROR("{}", message);
            break;
        case IPL_LOGLEVEL_DEBUG:
            LOG_DEBUG("{}", message);
            break;
    }
}

AAudioBuffer* AAudioBuffer::LoadAsset(const std::fpath& Path)
{
    return alContext->LoadAudioFile(Path);
}

AContext::AContext()
{
    LOG_INFO("creating audio context");

    IPLContextSettings ContextSettings{};
    ContextSettings.version = STEAMAUDIO_VERSION;
    ContextSettings.logCallback = &::SteamAudioLogCallback;

    iplCheckResult = iplContextCreate(&ContextSettings, &Context);

    HrtfSettings.type = IPL_HRTFTYPE_DEFAULT;

    AudioSettings.samplingRate = 44100;
    AudioSettings.frameSize = 1024;

    iplCheckResult = iplHRTFCreate(Context, &AudioSettings, &HrtfSettings, &hrtf);
}

AContext::~AContext()
{
    TaskExecutor.wait_for_all();
    LOG_INFO("destroying audio context");
    GarbageCollect();
    VERIFY(AudioBuffers.size() == 0, ASSERTION::NONFATAL);

    iplHRTFRelease(&hrtf);
    hrtf = nullptr;

    iplContextRelease(&Context);
    Context = nullptr;
}

AAudioBuffer* AContext::LoadAudioFile(const std::fpath& Path)
{
    auto[it, inserted] = AudioBuffers.emplace(Path, AAudioBuffer{});
    if(inserted)
    {
        TaskExecutor.silent_async([=]()
        {
            if(it->first.extension() == ".ogg")
            {
                LoadOggVorbisFile_Impl(TAssetPtr{it->first, &it->second});
            }
            else
            {
                VERIFY(false, "unsupported audio file format", it->first.extension());
            }
        });
    }

    return &it->second;
}

void AContext::LoadOggVorbisFile_Impl(TAssetPtr<AAudioBuffer> AudioBuffer)
{
    LOG_INFO("loading audio file {}", AudioBuffer.GetPath());

    AAudioBuffer* AudioObject = AudioBuffer.GetPtr();

    FILE* file = VERIFY(fopen(AudioBuffer.GetPath().c_str(), "r"), strerror(errno));

    OggVorbis_File VorbisFile{};
    VERIFY(ov_open_callbacks(file, &VorbisFile, nullptr, 0, OV_CALLBACKS_DEFAULT) == 0);

    vorbis_info* VorbisInfo = ov_info(&VorbisFile, -1);
    int current_section;

    std::vector<std::vector<float>> pcmChannels(VorbisInfo->channels);

    while(true)
    {
        float** pcm = nullptr;
        int64_t ret = ov_read_float(&VorbisFile, &pcm, INT32_MAX, &current_section);

        vorbis_info* NewInfo = ov_info(&VorbisFile, current_section);
        VERIFY(VorbisInfo->rate == NewInfo->rate && VorbisInfo->channels == NewInfo->channels);

        if(ret < 0)
        {
            LOG_WARNING("unexpected data encountered - {}", ret);
        }
        else if(ret == 0) //eof
        {
            break;
        }
        else
        {
            for(uint64_t channel = 0; channel < VorbisInfo->channels; ++channel)
            {
                std::vector<float>& pcmChannel = pcmChannels[channel];

                uint64_t oldSize = pcmChannel.size();
                pcmChannel.resize(oldSize + ret);
                memcpy(pcmChannel.data() + oldSize, pcm[channel], ret);
            }
        }
    }

    std::vector<float*> DataChannels(VorbisInfo->channels);
    for(uint64_t channel = 0; channel < VorbisInfo->channels; ++channel)
    {
        DataChannels[channel] = pcmChannels[channel].data();
    }

    IPLAudioBuffer Buffer{
        .numChannels = VorbisInfo->channels,
        .numSamples = 1024,
        .data = DataChannels.data()
    };

    LOG_INFO("finished loading audio file {}", AudioBuffer.GetPath());

    AudioObject->pcmChannels = std::move(pcmChannels);
    AudioObject->DataChannels = std::move(DataChannels);
    AudioObject->Buffer = Buffer;
    AudioObject->SetLoaded();

    ov_clear(&VorbisFile);
}

/*
void AContext::LoadOggVorbisFile_Impl(TAssetPtr<AAudioBuffer> AudioBuffer)
{
    LOG_INFO("loading audio file {}", AudioBuffer.GetPath());

    FILE* file = VERIFY(fopen(AudioBuffer.GetPath().c_str(), "r"), strerror(errno));

    OggVorbis_File VorbisFile{};
    VERIFY(ov_open_callbacks(file, &VorbisFile, nullptr, 0, OV_CALLBACKS_DEFAULT) == 0);

    vorbis_info* VorbisInfo = ov_info(&VorbisFile, -1);
    int current_section;

    AAudioBuffer* AudioObject = AudioBuffer.GetPtr();

    while(true)
    {
        uint64_t old_size = AudioObject->pcmBuffer.size();
        AudioObject->pcmBuffer.resize(old_size + 4096);

        int64_t ret = ov_read(&VorbisFile, AudioObject->pcmBuffer.data() + old_size, 4096, 0, 2, 1, &current_section);

        vorbis_info* NewInfo = ov_info(&VorbisFile, current_section);
        VERIFY(VorbisInfo->rate == NewInfo->rate);

        if(ret == 0) //eof
        {
            AudioObject->pcmBuffer.resize(old_size);
            break;
        }
        else if(ret < 0)
        {
            const char* reason = ret == OV_HOLE ? "error/hole in data" : ret == OV_EINVAL ? "partial open" : "unknown";
            LOG_WARNING("unexpected data encountered - {}", reason);
        }
        else
        {
            AudioObject->pcmBuffer.resize(old_size + ret);
        }
    }

    ov_clear(&VorbisFile);

    LOG_INFO("finished loading audio file {}", AudioBuffer.GetPath());

    AudioObject->pcmBuffer.shrink_to_fit();
    AudioObject->SetLoaded();
}
 */

void AContext::GarbageCollect()
{
    if(TaskExecutor.num_topologies() != 0)
    {
        return;
    }

    auto it = AudioBuffers.begin();
    while(it != AudioBuffers.end())
    {
        if(it->second.IsLoaded() && it->second.GetRefCount() == 0)
        {
            LOG_DEBUG("freeing audio file {}", it->first);
            it = AudioBuffers.unsafe_erase(it);
        }
        else
        {
            ++it;
        }
    }
}

IPLBinauralEffect AContext::CreateBinauralEffect()
{
    IPLBinauralEffectSettings EffectSettings{
            .hrtf = alContext->hrtf
    };

    IPLBinauralEffect ret{};
    iplCheckResult = iplBinauralEffectCreate(Context, &AudioSettings, &EffectSettings, &ret);
    return ret;
}
