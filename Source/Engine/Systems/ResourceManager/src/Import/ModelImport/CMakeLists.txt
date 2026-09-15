set(CLASS_NAME "ModelImport")

# The model import orchestrator: one ParseModel call fanned out to whichever
# importers the parse result calls for (mesh / skeleton / clips).
#
# Headers are API (ImportPipeline drives this unit), so they live under
# include/ResourceManager/Import/ModelImport/ and only sources are listed here --
# same layout as the sibling ModelParser unit.
#
# ModelAssetPlan.cpp names no assimp types and touches no files: it is the pure
# half, and t_ResourceManager_ModelAssetPlan drives it directly.
target_sources(ResourceManager PRIVATE
        ModelAssetPlan.cpp
        ModelImport.cpp
)
