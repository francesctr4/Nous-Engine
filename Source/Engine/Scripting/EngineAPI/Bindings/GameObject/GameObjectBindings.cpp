#include "Engine/Scripting/EngineAPI/Bindings/GameObject/GameObjectBindings.h"

#include "Engine/Scripting/iScriptSceneHost.h"
#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CTransform.h>
#include <Logger/Logger.h>

static IScriptSceneHost* s_scene = nullptr;

void SetupGameObjectBindings(GameObjectAPI& gameObject, IScriptSceneHost* sceneHost)
{
    s_scene = sceneHost;

    // Create binding - converts const char* to GameObjectID
    gameObject.Create = [](const char* name) -> uint32_t {
        if (!s_scene->GetActiveScene()) {
            NOUS_ERROR("No active scene to create GameObject in!");
            return 0;
        }
        GameObject go = s_scene->GetActiveScene()->CreateGameObject(name ? name : "GameObject");
        NOUS_DEBUG("Created GameObject '%s' with ID: %u", name, go.GetID());
        return go.GetID();
    };

    // Destroy binding - uses GameObjectID
    gameObject.Destroy = [](uint32_t id) {
        if (!s_scene->GetActiveScene()) {
            NOUS_ERROR("No active scene to destroy GameObject from!");
            return;
        }
        if (id == 0) {
            NOUS_WARN("Attempted to destroy GameObject with invalid ID 0");
            return;
        }
        NOUS_DEBUG("Destroying GameObject with ID: %u", id);
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (go.IsValid())
            s_scene->GetActiveScene()->DestroyGameObject(go);
    };

    gameObject.SetPosition = [](uint32_t id, float x, float y, float z) {
        if (!s_scene || !s_scene->GetActiveScene()) {
            NOUS_ERROR("Scene not available for SetPosition!");
            return;
        }
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid()) { NOUS_WARN("GameObject with ID %u not found for SetPosition!", id); return; }
        if (!go.HasComponent<CTransform>()) { NOUS_WARN("GameObject %u has no Transform component!", id); return; }
        auto& transform = go.GetComponent<CTransform>();
        transform.position = glm::vec3(x, y, z);
        transform.MarkDirty();
        transform.UpdateMatrix();
    };

    gameObject.SetRotation = [](uint32_t id, float x, float y, float z) {
        if (!s_scene || !s_scene->GetActiveScene()) {
            NOUS_ERROR("Scene not available for SetRotation!");
            return;
        }
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid()) { NOUS_WARN("GameObject with ID %u not found for SetRotation!", id); return; }
        if (!go.HasComponent<CTransform>()) { NOUS_WARN("GameObject %u has no Transform component!", id); return; }
        auto& transform = go.GetComponent<CTransform>();
        transform.SetEulerRotation(glm::vec3(x, y, z));
        transform.UpdateMatrix();
    };

    gameObject.SetScale = [](uint32_t id, float x, float y, float z) {
        if (!s_scene || !s_scene->GetActiveScene()) {
            NOUS_ERROR("Scene not available for SetScale!");
            return;
        }
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid()) { NOUS_WARN("GameObject ID %u not found for SetScale!", id); return; }
        if (!go.HasComponent<CTransform>()) { NOUS_WARN("GameObject %u has no Transform for SetScale!", id); return; }
        auto& transform = go.GetComponent<CTransform>();
        transform.scale = glm::vec3(x, y, z);
        transform.MarkDirty();
        transform.UpdateMatrix();
    };

    gameObject.GetPosition = [](uint32_t id, float* x, float* y, float* z) {
        if (!s_scene || !s_scene->GetActiveScene()) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid() || !go.HasComponent<CTransform>()) return;
        const auto& pos = go.GetComponent<CTransform>().position;
        if (x) *x = pos.x;
        if (y) *y = pos.y;
        if (z) *z = pos.z;
    };

    gameObject.GetRotation = [](uint32_t id, float* x, float* y, float* z) {
        if (!s_scene || !s_scene->GetActiveScene()) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid() || !go.HasComponent<CTransform>()) return;
        const glm::vec3 euler = go.GetComponent<CTransform>().GetEulerAngles();
        if (x) *x = euler.x;
        if (y) *y = euler.y;
        if (z) *z = euler.z;
    };

    gameObject.GetScale = [](uint32_t id, float* x, float* y, float* z) {
        if (!s_scene || !s_scene->GetActiveScene()) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid() || !go.HasComponent<CTransform>()) return;
        const auto& sc = go.GetComponent<CTransform>().scale;
        if (x) *x = sc.x;
        if (y) *y = sc.y;
        if (z) *z = sc.z;
    };

    gameObject.FindByName = [](const char* name) -> uint32_t {
        if (!s_scene || !s_scene->GetActiveScene() || !name) return 0;
        const GameObject go = s_scene->GetActiveScene()->FindGameObjectByName(name);
        return go.IsValid() ? go.GetID() : 0;
    };

    gameObject.GetForward = [](uint32_t id, float* x, float* y, float* z) {
        if (!s_scene || !s_scene->GetActiveScene()) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid() || !go.HasComponent<CTransform>()) return;
        const glm::vec3 fwd = go.GetComponent<CTransform>().GetForward();
        if (x) *x = fwd.x;
        if (y) *y = fwd.y;
        if (z) *z = fwd.z;
    };

    gameObject.GetRight = [](uint32_t id, float* x, float* y, float* z) {
        if (!s_scene || !s_scene->GetActiveScene()) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid() || !go.HasComponent<CTransform>()) return;
        const glm::vec3 right = go.GetComponent<CTransform>().GetRight();
        if (x) *x = right.x;
        if (y) *y = right.y;
        if (z) *z = right.z;
    };

    gameObject.GetUp = [](uint32_t id, float* x, float* y, float* z) {
        if (!s_scene || !s_scene->GetActiveScene()) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid() || !go.HasComponent<CTransform>()) return;
        const glm::vec3 up = go.GetComponent<CTransform>().GetUp();
        if (x) *x = up.x;
        if (y) *y = up.y;
        if (z) *z = up.z;
    };

    gameObject.Translate = [](uint32_t id, float dx, float dy, float dz) {
        if (!s_scene || !s_scene->GetActiveScene()) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid() || !go.HasComponent<CTransform>()) return;
        auto& transform = go.GetComponent<CTransform>();
        transform.Translate(glm::vec3(dx, dy, dz));
        transform.UpdateMatrix();
    };

    gameObject.Rotate = [](uint32_t id, float dx, float dy, float dz) {
        if (!s_scene || !s_scene->GetActiveScene()) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid() || !go.HasComponent<CTransform>()) return;
        auto& transform = go.GetComponent<CTransform>();
        transform.Rotate(glm::quat(glm::radians(glm::vec3(dx, dy, dz))));
        transform.UpdateMatrix();
    };

    gameObject.GetParent = [](uint32_t id) -> uint32_t {
        if (!s_scene || !s_scene->GetActiveScene()) return 0;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid()) return 0;
        const GameObject parent = go.GetParent();
        return parent.IsValid() ? parent.GetID() : 0;
    };

    gameObject.SetParent = [](uint32_t id, uint32_t parentId) {
        if (!s_scene || !s_scene->GetActiveScene()) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid()) { NOUS_WARN("[GameObjectAPI] SetParent: GameObject %u not found", id); return; }
        if (parentId == 0) {
            go.SetParent({});
        } else {
            GameObject parent = s_scene->GetActiveScene()->GetGameObjectByID(parentId);
            if (!parent.IsValid()) { NOUS_WARN("[GameObjectAPI] SetParent: parent %u not found", parentId); return; }
            go.SetParent(parent);
        }
    };

    gameObject.GetChildCount = [](uint32_t id) -> uint32_t {
        if (!s_scene || !s_scene->GetActiveScene()) return 0;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid()) return 0;
        return static_cast<uint32_t>(go.GetChildren().size());
    };

    gameObject.GetChild = [](uint32_t id, uint32_t index) -> uint32_t {
        if (!s_scene || !s_scene->GetActiveScene()) return 0;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid()) return 0;
        const auto children = go.GetChildren();
        if (index >= children.size()) return 0;
        return children[index].GetID();
    };

    gameObject.GetName = [](uint32_t id, char* buffer, int bufferSize) {
        if (!s_scene || !s_scene->GetActiveScene() || !buffer || bufferSize <= 0) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid()) return;
        const std::string& name = go.GetName();
        const int len = static_cast<int>(name.size());
        const int copyLen = (len < bufferSize - 1) ? len : bufferSize - 1;
        name.copy(buffer, copyLen);
        buffer[copyLen] = '\0';
    };

    gameObject.SetName = [](uint32_t id, const char* name) {
        if (!s_scene || !s_scene->GetActiveScene() || !name) return;
        GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
        if (!go.IsValid()) { NOUS_WARN("[GameObjectAPI] SetName: GameObject %u not found", id); return; }
        go.SetName(name);
    };
}
