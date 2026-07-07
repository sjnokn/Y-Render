#include "Scene/Scene.h"

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

void Scene::BuildDemo(int demoIndex, Mesh& cubeMesh, Mesh& planeMesh, Mesh& objMesh)
{
    Clear();

    if (demoIndex == 0)
    {
        SceneObject floor;
        floor.mesh = &planeMesh;
        floor.material.baseColor = XMFLOAT4(0.72f, 0.72f, 0.68f, 1.0f);
        floor.material.specularPower = 18.0f;
        floor.transform.position = XMFLOAT3(0.0f, -1.0f, 0.0f);
        Add(floor);

        SceneObject cube;
        cube.mesh = &cubeMesh;
        cube.material.baseColor = XMFLOAT4(0.85f, 0.65f, 0.32f, 1.0f);
        cube.transform.position = XMFLOAT3(0.0f, 0.2f, 0.0f);
        cube.transform.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
        Add(cube);
    }
    else
    {
        SceneObject obj;
        obj.mesh = &objMesh;
        obj.material.baseColor = XMFLOAT4(0.45f, 0.74f, 0.95f, 1.0f);
        obj.material.specularPower = 72.0f;
        obj.transform.position = XMFLOAT3(0.0f, -0.7f, 0.0f);
        obj.transform.scale = XMFLOAT3(1.4f, 1.4f, 1.4f);
        Add(obj);
    }
}

void Scene::Animate(float time, int demoIndex)
{
    if (demoIndex == 0 && m_objects.size() > 1)
    {
        m_objects[1].transform.rotation.y = time * 0.75f;
    }
    if (demoIndex == 1 && !m_objects.empty())
    {
        m_objects[0].transform.rotation.y = time * 0.55f;
    }
}
} // namespace YRender
