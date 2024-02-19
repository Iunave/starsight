#ifndef STARSIGHT_AUDIO_MODULE_HPP
#define STARSIGHT_AUDIO_MODULE_HPP

#include "flecs.h"
#include "soundcue_component.hpp"

class AudioModule
{
private:
    static inline constinit AudioModule* Self = nullptr;

    static void GarbageCollect(flecs::iter&);
    static void PlaySoundCues(SoundCueComponent& SoundCue);

public:
    explicit AudioModule(flecs::world& world);
    ~AudioModule();
};

#endif //STARSIGHT_AUDIO_MODULE_HPP
