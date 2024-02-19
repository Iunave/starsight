#ifndef STARSIGHT_MODEL_COMPONENT_HPP
#define STARSIGHT_MODEL_COMPONENT_HPP

#include "core/resource.hpp"
#include "render/vk_model.hpp"

struct ModelComponent
{
    TAssetPtr<VModel> Asset{};
    bool isBuilt = false;
};

#endif //STARSIGHT_MODEL_COMPONENT_HPP
