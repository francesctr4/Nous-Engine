#ifndef NOUS_ENGINE_INSPECTORCSCRIPT_H
#define NOUS_ENGINE_INSPECTORCSCRIPT_H

#include <cstdint>
#include <string>

class ScriptManager;
class Scene;
struct ScriptProperty;
class CScript;

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void DrawAttachedScripts(Scene* scene, CScript* cscript);

bool DrawSingleScript(Scene* scene, CScript* cscript, int i, const std::string& sName);

void DrawScriptProperties(Scene* scene, const CScript* cscript, int i);

void DrawScriptProperty(Scene* scene, const ScriptProperty& prop);

void DrawGameObjectProperty(Scene* scene, const ScriptProperty& prop);

std::string GetGameObjectPreview(const uint32_t* idPtr, Scene* scene);

void DrawGameObjectOptions(uint32_t* idPtr, const Scene* scene);

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

void DrawAddScriptSection(const ScriptManager* scriptManager, CScript* cscript);

// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------

#endif //NOUS_ENGINE_INSPECTORCSCRIPT_H
