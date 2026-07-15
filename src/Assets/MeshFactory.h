#pragma once

#include "Scene/SceneTypes.h"

#include <string>

namespace YRender
{
bool LoadStaticMesh(const std::wstring& path, Mesh& outMesh);
bool LoadObj(const std::wstring& path, Mesh& outMesh);
bool LoadGltf(const std::wstring& path, Mesh& outMesh);
void NormalizeMeshForPreview(Mesh& mesh, float targetHeight = 2.4f);
void UpdateMeshBounds(Mesh& mesh);
Mesh CreateCubeMesh();
Mesh CreateDogMesh();
Mesh CreatePlaneMesh();
} // namespace YRender
