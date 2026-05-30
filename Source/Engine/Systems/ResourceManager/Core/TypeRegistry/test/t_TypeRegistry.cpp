#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Core/TypeRegistry/include/TypeRegistry.h"
#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporter.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/Resource.h"
#include "Engine/Systems/ResourceManager/Types/ResourceType.h"

#include <string>
#include <vector>

// TypeRegistry is directly constructible and injected into its consumers via
// constructor (Application owns the one production instance), so these tests
// operate on a local instance with no MemoryManager / JobSystem / EventSystem
// setup. Descriptors own their
// importers via unique_ptr<IAssetImporter>, freed with the registry — nothing
// here goes through NOUS_NEW, so there is no leak-abort surface.
//
// The Register() runtime-object invariant asserts (createFn/destroyFn/
// resourceImporter must agree) abort the process on failure, so they are not
// death-tested here; the cases below exercise the two *valid* shapes.

namespace
{
    // Pipeline-only importer: Import only, no runtime resource object.
    struct TestAssetImporter final : IAssetImporter
    {
        bool Import(const MetaFileData&) override { return true; }
    };

    // Full runtime-lifecycle importer.
    struct TestResourceImporter final : IResourceImporter
    {
        bool Import(const MetaFileData&) override                    { return true; }
        bool Save(const MetaFileData&, Resource*&) override          { return true; }
        bool Deserialize(const std::string&, Resource*) override     { return true; }
        void Evict(Resource*) override                               {}
        bool Upload(Resource*, IGPUResourceFactory*) override        { return true; }
        void Release(Resource*, IGPUResourceFactory*) override       {}
    };

    // A resource type: createFn + destroyFn + an IResourceImporter, all set.
    TypeDescriptor MakeResourceType(ResourceType type, const char* name,
                                    std::vector<std::string> exts, int cleanupPriority)
    {
        TypeDescriptor d;
        d.type             = type;
        d.name             = name;
        d.sourceExtensions = std::move(exts);
        d.cleanupPriority  = cleanupPriority;
        d.SetImporter<TestResourceImporter>();
        d.createFn  = [](uint32) -> Resource* { return nullptr; };
        d.destroyFn = [](Resource*) {};
        return d;
    }

    // A pipeline-only type (like SCENE): just an IAssetImporter, no create/destroy.
    TypeDescriptor MakePipelineType(ResourceType type, const char* name,
                                    std::vector<std::string> exts, int cleanupPriority)
    {
        TypeDescriptor d;
        d.type             = type;
        d.name             = name;
        d.sourceExtensions = std::move(exts);
        d.cleanupPriority  = cleanupPriority;
        d.SetImporter<TestAssetImporter>();
        return d;
    }
}

// ---- Register / Get ---------------------------------------------------------

TEST(TypeRegistry, RegisterThenGetReturnsDescriptor)
{
    TypeRegistry reg;
    reg.Register(MakeResourceType(ResourceType::MESH, "Mesh", { "fbx" }, 0));

    const TypeDescriptor* d = reg.Get(ResourceType::MESH);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->type, ResourceType::MESH);
    EXPECT_STREQ(d->name, "Mesh");
}

TEST(TypeRegistry, GetSentinelOrUnregisteredTypesReturnsNull)
{
    TypeRegistry reg;
    reg.Register(MakeResourceType(ResourceType::MESH, "Mesh", { "fbx" }, 0));

    EXPECT_EQ(reg.Get(ResourceType::UNKNOWN),   nullptr);
    EXPECT_EQ(reg.Get(ResourceType::ALL_TYPES), nullptr);
    EXPECT_EQ(reg.Get(ResourceType::TEXTURE),   nullptr); // never registered
}

// ---- SetImporter wiring (the post-refactor invariant) -----------------------

TEST(TypeRegistry, ResourceTypeWiresBothImporterHandles)
{
    TypeRegistry reg;
    reg.Register(MakeResourceType(ResourceType::MESH, "Mesh", { "fbx" }, 0));

    const TypeDescriptor* d = reg.Get(ResourceType::MESH);
    ASSERT_NE(d, nullptr);
    EXPECT_NE(d->importer.get(),   nullptr);
    EXPECT_NE(d->resourceImporter, nullptr);
    // resourceImporter is a non-owning alias into the same object as importer.
    EXPECT_EQ(static_cast<IAssetImporter*>(d->resourceImporter), d->importer.get());
}

TEST(TypeRegistry, PipelineOnlyTypeLeavesResourceImporterNull)
{
    TypeRegistry reg;
    reg.Register(MakePipelineType(ResourceType::SCENE, "Scene", { "nous" }, 0));

    const TypeDescriptor* d = reg.Get(ResourceType::SCENE);
    ASSERT_NE(d, nullptr);
    EXPECT_NE(d->importer.get(),   nullptr); // still imports
    EXPECT_EQ(d->resourceImporter, nullptr); // but has no runtime lifecycle
    EXPECT_EQ(d->createFn,  nullptr);
    EXPECT_EQ(d->destroyFn, nullptr);
}

// ---- Extension lookup -------------------------------------------------------

TEST(TypeRegistry, TypeFromExtensionMatchesWithAndWithoutDot)
{
    TypeRegistry reg;
    reg.Register(MakeResourceType(ResourceType::MESH,    "Mesh",    { "fbx", "obj" }, 0));
    reg.Register(MakeResourceType(ResourceType::TEXTURE, "Texture", { "png" },        1));

    EXPECT_EQ(reg.TypeFromExtension("fbx"),  ResourceType::MESH);
    EXPECT_EQ(reg.TypeFromExtension(".fbx"), ResourceType::MESH);
    EXPECT_EQ(reg.TypeFromExtension("obj"),  ResourceType::MESH);
    EXPECT_EQ(reg.TypeFromExtension("png"),  ResourceType::TEXTURE);
}

TEST(TypeRegistry, TypeFromExtensionUnknownOrEmptyReturnsUnknown)
{
    TypeRegistry reg;
    reg.Register(MakeResourceType(ResourceType::MESH, "Mesh", { "fbx" }, 0));

    EXPECT_EQ(reg.TypeFromExtension("xyz"), ResourceType::UNKNOWN);
    EXPECT_EQ(reg.TypeFromExtension(""),    ResourceType::UNKNOWN);
}

// ---- Iteration & cleanup ordering -------------------------------------------

TEST(TypeRegistry, AllReturnsOnlyRegisteredDescriptors)
{
    TypeRegistry reg;
    reg.Register(MakeResourceType(ResourceType::MESH,    "Mesh",    { "fbx" }, 0));
    reg.Register(MakeResourceType(ResourceType::TEXTURE, "Texture", { "png" }, 1));

    EXPECT_EQ(reg.All().size(), 2u);
}

TEST(TypeRegistry, SortedByCleanupPriorityIsAscendingAndStable)
{
    TypeRegistry reg;
    // Register out of priority order; expect ascending priority on read-back.
    reg.Register(MakeResourceType(ResourceType::MESH,     "Mesh",     { "fbx" }, 3));
    reg.Register(MakeResourceType(ResourceType::SHADER,   "Shader",   { "glsl" }, 0));
    reg.Register(MakeResourceType(ResourceType::MATERIAL, "Material", { "nmat" }, 1));

    const std::vector<const TypeDescriptor*> sorted = reg.SortedByCleanupPriority();
    ASSERT_EQ(sorted.size(), 3u);
    EXPECT_EQ(sorted[0]->type, ResourceType::SHADER);   // prio 0
    EXPECT_EQ(sorted[1]->type, ResourceType::MATERIAL); // prio 1
    EXPECT_EQ(sorted[2]->type, ResourceType::MESH);     // prio 3

    for (size_t i = 1; i < sorted.size(); ++i)
        EXPECT_LE(sorted[i - 1]->cleanupPriority, sorted[i]->cleanupPriority);
}
