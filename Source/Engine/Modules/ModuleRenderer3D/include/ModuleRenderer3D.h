#ifndef NOUS_ENGINE_MODULE_RENDERER3D_H
#define NOUS_ENGINE_MODULE_RENDERER3D_H

// -----------------------------------------------------------------------------
// ModuleRenderer3D
// -----------------------------------------------------------------------------
//
// The 3D Renderer Module is responsible for managing the rendering pipeline.
// It acts as a bridge between the engine and the chosen renderer backend
// (Vulkan, OpenGL, DirectX, etc.).
//
// Responsibilities:
//  - Initialize and shutdown the renderer frontend/backend.
//  - Collect and build render packets each frame.
//  - Handle window resize and other render-related events.
// -----------------------------------------------------------------------------

#include "Engine/Modules/Module.h"
#include "Engine/Core/Event System/IEventListener.h"
#include "Engine/EngineExport.h"

// ---------------------------------------------------------------------
// Forward Declarations
// ---------------------------------------------------------------------
struct RenderPacket;
class RendererFrontend;

class ModuleRenderer3D : public Module, public IEventListener
{
public:
	// ---------------------------------------------------------------------
	// Constructor / Destructor
	// ---------------------------------------------------------------------
	explicit ModuleRenderer3D(Application* app);
	~ModuleRenderer3D() override;

	// ---------------------------------------------------------------------
	// Lifecycle
	// ---------------------------------------------------------------------
	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	// ---------------------------------------------------------------------
	// Events
	// ---------------------------------------------------------------------
	void OnEvent(const Event& event) override;

	// ---------------------------------------------------------------------
	// Accessors
	// ---------------------------------------------------------------------
	[[nodiscard]] NOUS_ENGINE_API RendererFrontend* GetRendererFrontend() const;

private:
	// ---------------------------------------------------------------------
	// Internal Methods
	// ---------------------------------------------------------------------
	[[nodiscard]] bool BuildRenderPacket(RenderPacket* packet);

private:
	// ---------------------------------------------------------------------
	// Members
	// ---------------------------------------------------------------------
	RendererFrontend* mRendererFrontend;
};

#endif // NOUS_ENGINE_MODULE_RENDERER3D_H
