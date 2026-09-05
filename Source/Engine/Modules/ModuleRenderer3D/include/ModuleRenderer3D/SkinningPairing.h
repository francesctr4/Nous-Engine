#pragma once

#include <EngineCore/EngineExport.h>
#include <Renderer/RendererTypes.h>

#include <entt/entt.hpp>

class ResourceMesh;

// Fills a geometry's palette and model matrix when it is a skinned mesh whose
// animator has a usable pose; leaves both untouched otherwise.
//
// BuildModelHierarchyInto creates exactly one level -- a root GameObject with no
// mesh, one child per submesh -- so a skinned mesh's CAnimator is on its DIRECT
// parent. A single lookup, never a chain walk.
//
// The model matrix becomes the ANIMATOR ROOT's, not the child's: the palette already
// maps mesh space to animated model space, so the child's own FBX node transform
// would be applied twice. Mixamo's children are identity, so the wrong version looks
// correct and only breaks on a model authored with a real offset.
//
// It is a free function on a public header rather than a private helper because FOUR
// builders need the identical pairing and every one of them is a place it can be
// forgotten: the scene packet, the outlined-geometry list, the normals visualization,
// and -- the one that was missed -- the editor's mouse-pick list in SceneViewport,
// which is a hand-copy of the scene builder living in another target entirely.
NOUS_ENGINE_API void ApplySkinningToGeometry(const entt::registry& registry,
                                             entt::entity entity,
                                             const ResourceMesh& mesh,
                                             GeometryRenderData& data);
