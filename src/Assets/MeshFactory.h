#pragma once

#include "Scene/SceneTypes.h"

#include <string>

namespace YRender
{
bool LoadObj(const std::wstring& path, Mesh& outMesh);
Mesh CreateCubeMesh();
Mesh CreatePlaneMesh();
} // namespace YRender
