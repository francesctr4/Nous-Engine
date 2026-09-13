set(CLASS_NAME "ModelParser")

# The model pre-pass. Headers are API (ImportPipeline drives this unit and the
# importers consume its output), so they live under
# include/ResourceManager/Import/ModelParser/ and only the sources are here.
#
# SkeletonBuild.cpp and ClipBuild.cpp name NO assimp types on purpose -- see the
# header comments. ModelParser.cpp is the one translation unit in the engine that
# includes <assimp/*>; keep it that way.
target_sources(ResourceManager PRIVATE
        SkeletonBuild.cpp
        ClipBuild.cpp
        ModelParser.cpp
)
