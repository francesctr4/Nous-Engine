#include <ResourceManager/Types/ResourceAnimationController/ResourceAnimationController.h>

ResourceAnimationController::ResourceAnimationController(const uint32_t uid)
    : ResourceBase(uid, ResourceType::ANIMATION_CONTROLLER)
{
}

ResourceAnimationController::~ResourceAnimationController() = default;
