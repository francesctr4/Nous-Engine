#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ResourceMesh.h"

// =====================================================
// Tests — ResourceMesh
// =====================================================

TEST(t_ResourceMesh, ConstructorSetsTypeMeshAndUID)
{
    ResourceMesh mesh(42);
    EXPECT_EQ(mesh.GetType(), ResourceType::MESH);
    EXPECT_EQ(mesh.GetUID(), 42u);
}

TEST(t_ResourceMesh, VerticesAndIndicesDefaultToEmpty)
{
    ResourceMesh mesh;
    EXPECT_TRUE(mesh.vertices.empty());
    EXPECT_TRUE(mesh.indices.empty());
}

TEST(t_ResourceMesh, RefCountStartsAtZero)
{
    ResourceMesh mesh(1);
    EXPECT_EQ(mesh.GetReferenceCount(), 0u);
}
