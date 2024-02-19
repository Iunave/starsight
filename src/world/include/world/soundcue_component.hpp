#ifndef STARSIGHT_SOUNDCUE_COMPONENT_HPP
#define STARSIGHT_SOUNDCUE_COMPONENT_HPP

#include "audio/audio_context.hpp"

class SoundCueComponent
{
public:
    TAssetPtr<AAudioBuffer> Asset{};
};

#endif //STARSIGHT_SOUNDCUE_COMPONENT_HPP
