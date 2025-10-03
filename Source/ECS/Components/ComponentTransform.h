#ifndef NOUS_ENGINE_COMPONENTTRANSFORM_H
#define NOUS_ENGINE_COMPONENTTRANSFORM_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

struct Transform {
    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation {0.0f, 0.0f, 0.0f};
    glm::vec3 scale    {1.0f, 1.0f, 1.0f};

    // Cached world matrix (updated by a system)
    glm::mat4 worldMatrix {1.0f};
};

#endif //NOUS_ENGINE_COMPONENTTRANSFORM_H
