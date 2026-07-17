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
    void BuildDemo(int demoIndex, Mesh& characterMesh, Mesh& planeMesh, bool externalCharacterLoaded);
    void Animate(float deltaSeconds, bool rotateCharacter);

    const std::vector<SceneObject>& Objects() const { return m_objects; }
    std::vector<SceneObject>& Objects() { return m_objects; }
    const std::vector<DirectionalLight>& DirectionalLights() const { return m_directionalLights; }
    std::vector<DirectionalLight>& DirectionalLights() { return m_directionalLights; }

private:
    std::vector<SceneObject> m_objects;
    std::vector<DirectionalLight> m_directionalLights;
};
} // namespace YRender
