#pragma once

#include "Scene/SceneTypes.h"

#include <vector>

namespace YRender
{
class Scene
{
public:
    void Clear();
    void Add(const SceneObject& object);
    void BuildDemo(int demoIndex, Mesh& cubeMesh, Mesh& planeMesh, Mesh& objMesh);
    void Animate(float time, int demoIndex);

    const std::vector<SceneObject>& Objects() const { return m_objects; }
    std::vector<SceneObject>& Objects() { return m_objects; }

private:
    std::vector<SceneObject> m_objects;
};
} // namespace YRender
