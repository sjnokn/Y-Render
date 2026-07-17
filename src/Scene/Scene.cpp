#include "Scene/Scene.h"

#include <cmath>

using namespace DirectX;

namespace YRender
{
void Scene::Clear()
{
    m_objects.clear();
}

void Scene::Add(const SceneObject& object)
{
    m_objects.push_back(object);
}

void Scene::BuildDemo(int demoIndex, Mesh& characterMesh, Mesh& planeMesh, bool externalCharacterLoaded)
{
    Clear();
    m_directionalLights.clear();
    if (demoIndex == 0)
    {
        m_directionalLights.push_back({XMFLOAT3(-0.40f, -0.82f, 0.28f), 1.10f, XMFLOAT3(1.0f, 0.93f, 0.82f), 0.0f});
        m_directionalLights.push_back({XMFLOAT3(0.72f, -0.36f, -0.42f), 0.38f, XMFLOAT3(0.42f, 0.60f, 1.0f), 0.0f});
    }
    else if (demoIndex == 1)
    {
        m_directionalLights.push_back({XMFLOAT3(-0.55f, -0.72f, 0.42f), 1.00f, XMFLOAT3(1.0f, 0.82f, 0.72f), 0.0f});
        m_directionalLights.push_back({XMFLOAT3(0.62f, -0.25f, -0.58f), 0.55f, XMFLOAT3(0.38f, 0.72f, 1.0f), 0.0f});
    }
    else if (demoIndex == 2)
    {
        m_directionalLights.push_back({XMFLOAT3(-0.28f, -0.78f, 0.50f), 1.15f, XMFLOAT3(1.0f, 0.70f, 0.42f), 0.0f});
        m_directionalLights.push_back({XMFLOAT3(0.58f, -0.32f, -0.48f), 0.42f, XMFLOAT3(0.36f, 0.48f, 1.0f), 0.0f});
    }
    else
    {
        m_directionalLights.push_back({XMFLOAT3(-0.42f, -0.76f, 0.38f), 1.05f, XMFLOAT3(0.92f, 0.96f, 1.0f), 0.0f});
        m_directionalLights.push_back({XMFLOAT3(0.55f, -0.28f, -0.50f), 0.34f, XMFLOAT3(0.42f, 0.72f, 0.86f), 0.0f});
    }

    SceneObject floor;
    floor.name = "Showcase Ground";
    floor.mesh = &planeMesh;
    floor.material.baseColor = demoIndex == 0 ? XMFLOAT4(0.58f, 0.74f, 0.74f, 1.0f) : (demoIndex == 1 ? XMFLOAT4(0.58f, 0.70f, 0.76f, 1.0f) : (demoIndex == 2 ? XMFLOAT4(0.52f, 0.72f, 0.66f, 1.0f) : XMFLOAT4(0.55f, 0.72f, 0.76f, 1.0f)));
    floor.material.ambientColor = XMFLOAT4(0.22f, 0.28f, 0.28f, 1.0f);
    floor.material.diffuseIntensity = 0.62f;
    floor.material.specularIntensity = 0.18f;
    floor.material.specularPower = 32.0f;
    floor.material.useAlbedoTexture = false;
    floor.transform.position = XMFLOAT3(0.0f, -1.0f, 0.0f);
    Add(floor);

    SceneObject character;
    character.name = demoIndex == 0 ? "Basic Lighting Character" : (demoIndex == 1 ? "Toon Character" : (demoIndex == 2 ? "Dissolve Character" : "Depth Fog Character"));
    if (!externalCharacterLoaded)
    {
        character.name += " (Fallback)";
    }
    character.mesh = &characterMesh;
    character.material.baseColor = externalCharacterLoaded ? XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) : XMFLOAT4(0.45f, 0.74f, 0.95f, 1.0f);
    character.material.ambientColor = demoIndex == 0 ? XMFLOAT4(0.13f, 0.15f, 0.19f, 1.0f) : (demoIndex == 1 ? XMFLOAT4(0.20f, 0.16f, 0.24f, 1.0f) : (demoIndex == 2 ? XMFLOAT4(0.10f, 0.08f, 0.16f, 1.0f) : XMFLOAT4(0.16f, 0.20f, 0.24f, 1.0f)));
    character.material.diffuseIntensity = 1.0f;
    character.material.specularIntensity = demoIndex == 0 ? 0.24f : (demoIndex == 1 ? 0.10f : (demoIndex == 2 ? 0.18f : 0.28f));
    character.material.specularPower = 56.0f;
    character.material.shadowColor = XMFLOAT4(0.16f, 0.12f, 0.23f, 1.0f);
    character.material.rimColor = XMFLOAT4(0.62f, 0.88f, 1.0f, 1.0f);
    character.material.lightingModel = demoIndex == 1 ? 1 : 0;
    character.material.surfaceEffect = 0;
    character.material.toonSteps = 4.0f;
    character.material.rimPower = 2.8f;
    character.material.rimIntensity = demoIndex == 1 ? 0.62f : 0.0f;
    character.material.dissolveAmount = 0.0f;
    character.material.dissolveNoiseScale = 3.2f;
    character.material.dissolveEdgeWidth = 0.055f;
    character.material.dissolveSpeed = 0.20f;
    character.material.dissolveEdgeIntensity = 1.35f;
    character.material.dissolveProgressSpeed = 0.16f;
    character.material.dissolveAutoProgress = false;
    character.material.dissolveEdgeColor = XMFLOAT4(0.18f, 0.76f, 1.0f, 1.0f);
    if (demoIndex == 2)
    {
        ApplyDissolvePreset(character.material);
    }
    else if (demoIndex == 3)
    {
        character.material.surfaceEffect = 2;
        character.material.rimColor = XMFLOAT4(0.42f, 0.82f, 0.96f, 1.0f);
    }
    character.material.useAlbedoTexture = true;
    character.transform.position = XMFLOAT3(0.0f, -1.0f, 0.0f);
    character.transform.rotation = XMFLOAT3(0.0f, XM_PI, 0.0f);
    Add(character);
}

void Scene::Animate(float deltaSeconds, bool rotateCharacter)
{
    for (SceneObject& object : m_objects)
    {
        Material& material = object.material;
        if (material.surfaceEffect == 1 && material.dissolveAutoProgress && !material.dissolvePaused)
        {
            material.dissolvePlaybackTime += deltaSeconds;
        }
    }

    if (rotateCharacter && m_objects.size() > 1)
    {
        SceneObject& character = m_objects[1];
        character.transform.rotation.y = std::fmod(character.transform.rotation.y + deltaSeconds * 0.32f, XM_2PI);
    }
}
} // namespace YRender
