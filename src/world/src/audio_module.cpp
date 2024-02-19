#include "audio_module.hpp"

AudioModule::AudioModule(flecs::world& world)
{
    Self = this;
    world.module<AudioModule>("Audio");

    world.component<SoundCueComponent>("Sound Cue");

    world.system<SoundCueComponent>("Play Sound Cues")
            .kind(flecs::OnUpdate)
            .each(PlaySoundCues);

    world.system("Audio Context GC")
            .kind(flecs::PostUpdate)
            .interval(10)
            .iter(GarbageCollect);
}

AudioModule::~AudioModule()
{
    Self = nullptr;
}

void AudioModule::GarbageCollect(flecs::iter&)
{
    alContext->GarbageCollect();
}

void AudioModule::PlaySoundCues(SoundCueComponent& SoundCue)
{
    if(!SoundCue.Asset.IsLoaded()) [[unlikely]]
    {
        SoundCue.Asset.Load();
        return;
    }
}
