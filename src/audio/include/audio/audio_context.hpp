#ifndef STARSIGHT_AUDIO_CONTEXT_HPP
#define STARSIGHT_AUDIO_CONTEXT_HPP

#include <cstdint>
#include "steamaudio/include/phonon.h"
#include "core/resource.hpp"
#include "core/math.hpp"
#include "taskflow/taskflow.hpp"
#include "tbb/concurrent_unordered_map.h"
#include "core/filesystem.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <atomic>
#include <filesystem>

class AAudioBuffer : public SharedAsset
{
public:
    static AAudioBuffer* LoadAsset(const std::fpath& Path);
public:

    std::vector<std::vector<float>> pcmChannels{};
    std::vector<float*> DataChannels{};
    IPLAudioBuffer Buffer{};
    std::atomic_bool bLoaded = false;

    AAudioBuffer() = default;
    AAudioBuffer(AAudioBuffer&& Other)
        : pcmChannels(std::move(Other.pcmChannels))
        , DataChannels(std::move(Other.DataChannels))
        , Buffer(Other.Buffer)
        , bLoaded(Other.IsLoaded())
    {
    }

    void SetLoaded()
    {
        bLoaded.store(true, std::memory_order_release);
        bLoaded.notify_all();
    }

    bool IsLoaded() const
    {
        return bLoaded.load(std::memory_order_acquire);
    }

    void WaitUntilLoaded() const
    {
        bLoaded.wait(false, std::memory_order_acquire);
    }
};

class AContext
{
private:
    friend class AAudioBuffer;

    IPLContext Context = nullptr;
    IPLHRTF hrtf = nullptr;
    IPLHRTFSettings HrtfSettings{};
    IPLAudioSettings AudioSettings{};

    tbb::concurrent_unordered_map<std::filesystem::path, AAudioBuffer> AudioBuffers{};
    tf::Executor TaskExecutor{};

public:

    AContext();
    ~AContext();

    IPLBinauralEffect CreateBinauralEffect();

    AAudioBuffer* LoadAudioFile(const std::fpath& Path);

    void GarbageCollect();

private:

    static void LoadOggVorbisFile_Impl(TAssetPtr<AAudioBuffer> AudioBuffer);
};

inline AContext* alContext = nullptr;

#endif //STARSIGHT_AUDIO_CONTEXT_HPP
