#include "Assets/MeshFactory.h"

#include <array>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace YRender
{
namespace
{
struct ObjKey
{
    int position = 0;
    int texcoord = 0;
    int normal = 0;

    bool operator==(const ObjKey& other) const
    {
        return position == other.position && texcoord == other.texcoord && normal == other.normal;
    }
};

struct ObjKeyHash
{
    size_t operator()(const ObjKey& key) const
    {
        return static_cast<size_t>((key.position * 73856093) ^ (key.texcoord * 19349663) ^ (key.normal * 83492791));
    }
};

ObjKey ParseObjKey(const std::string& text)
{
    ObjKey key{};
    std::stringstream stream(text);
    std::string item;

    if (std::getline(stream, item, '/'))
    {
        key.position = item.empty() ? 0 : std::stoi(item);
    }
    if (std::getline(stream, item, '/'))
    {
        key.texcoord = item.empty() ? 0 : std::stoi(item);
    }
    if (std::getline(stream, item, '/'))
    {
        key.normal = item.empty() ? 0 : std::stoi(item);
    }

    return key;
}

template <typename T>
const T& ObjAt(const std::vector<T>& values, int objIndex, const T& fallback)
{
    if (objIndex == 0)
    {
        return fallback;
    }

    const int resolved = objIndex > 0 ? objIndex - 1 : static_cast<int>(values.size()) + objIndex;
    if (resolved < 0 || resolved >= static_cast<int>(values.size()))
    {
        return fallback;
    }

    return values[static_cast<size_t>(resolved)];
}
} // namespace

bool LoadObj(const std::wstring& path, Mesh& outMesh)
{
    std::ifstream file(path);
    if (!file)
    {
        return false;
    }

    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<DirectX::XMFLOAT3> normals;
    std::vector<DirectX::XMFLOAT2> texcoords;
    std::unordered_map<ObjKey, uint32_t, ObjKeyHash> vertexMap;

    const DirectX::XMFLOAT3 fallbackPosition{0.0f, 0.0f, 0.0f};
    const DirectX::XMFLOAT3 fallbackNormal{0.0f, 1.0f, 0.0f};
    const DirectX::XMFLOAT2 fallbackUv{0.0f, 0.0f};

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "v")
        {
            DirectX::XMFLOAT3 value{};
            stream >> value.x >> value.y >> value.z;
            positions.push_back(value);
        }
        else if (type == "vn")
        {
            DirectX::XMFLOAT3 value{};
            stream >> value.x >> value.y >> value.z;
            normals.push_back(value);
        }
        else if (type == "vt")
        {
            DirectX::XMFLOAT2 value{};
            stream >> value.x >> value.y;
            value.y = 1.0f - value.y;
            texcoords.push_back(value);
        }
        else if (type == "f")
        {
            std::vector<uint32_t> faceIndices;
            std::string token;
            while (stream >> token)
            {
                ObjKey key = ParseObjKey(token);
                auto found = vertexMap.find(key);
                if (found == vertexMap.end())
                {
                    Vertex vertex{};
                    vertex.position = ObjAt(positions, key.position, fallbackPosition);
                    vertex.normal = ObjAt(normals, key.normal, fallbackNormal);
                    vertex.uv = ObjAt(texcoords, key.texcoord, fallbackUv);
                    const uint32_t newIndex = static_cast<uint32_t>(outMesh.vertices.size());
                    outMesh.vertices.push_back(vertex);
                    vertexMap.emplace(key, newIndex);
                    faceIndices.push_back(newIndex);
                }
                else
                {
                    faceIndices.push_back(found->second);
                }
            }

            for (size_t i = 1; i + 1 < faceIndices.size(); ++i)
            {
                outMesh.indices.push_back(faceIndices[0]);
                outMesh.indices.push_back(faceIndices[i]);
                outMesh.indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    return !outMesh.vertices.empty() && !outMesh.indices.empty();
}

Mesh CreateCubeMesh()
{
    Mesh mesh;
    const std::array<Vertex, 24> vertices = {
        Vertex{{-1, -1, -1}, {0, 0, -1}, {0, 1}}, Vertex{{-1, 1, -1}, {0, 0, -1}, {0, 0}}, Vertex{{1, 1, -1}, {0, 0, -1}, {1, 0}}, Vertex{{1, -1, -1}, {0, 0, -1}, {1, 1}},
        Vertex{{1, -1, 1}, {0, 0, 1}, {0, 1}}, Vertex{{1, 1, 1}, {0, 0, 1}, {0, 0}}, Vertex{{-1, 1, 1}, {0, 0, 1}, {1, 0}}, Vertex{{-1, -1, 1}, {0, 0, 1}, {1, 1}},
        Vertex{{-1, -1, 1}, {-1, 0, 0}, {0, 1}}, Vertex{{-1, 1, 1}, {-1, 0, 0}, {0, 0}}, Vertex{{-1, 1, -1}, {-1, 0, 0}, {1, 0}}, Vertex{{-1, -1, -1}, {-1, 0, 0}, {1, 1}},
        Vertex{{1, -1, -1}, {1, 0, 0}, {0, 1}}, Vertex{{1, 1, -1}, {1, 0, 0}, {0, 0}}, Vertex{{1, 1, 1}, {1, 0, 0}, {1, 0}}, Vertex{{1, -1, 1}, {1, 0, 0}, {1, 1}},
        Vertex{{-1, 1, -1}, {0, 1, 0}, {0, 1}}, Vertex{{-1, 1, 1}, {0, 1, 0}, {0, 0}}, Vertex{{1, 1, 1}, {0, 1, 0}, {1, 0}}, Vertex{{1, 1, -1}, {0, 1, 0}, {1, 1}},
        Vertex{{-1, -1, 1}, {0, -1, 0}, {0, 1}}, Vertex{{-1, -1, -1}, {0, -1, 0}, {0, 0}}, Vertex{{1, -1, -1}, {0, -1, 0}, {1, 0}}, Vertex{{1, -1, 1}, {0, -1, 0}, {1, 1}},
    };

    const std::array<uint32_t, 36> indices = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    };

    mesh.vertices.assign(vertices.begin(), vertices.end());
    mesh.indices.assign(indices.begin(), indices.end());
    return mesh;
}

Mesh CreatePlaneMesh()
{
    Mesh mesh;
    mesh.vertices = {
        {{-4.0f, 0.0f, -4.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 4.0f}},
        {{-4.0f, 0.0f, 4.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{4.0f, 0.0f, 4.0f}, {0.0f, 1.0f, 0.0f}, {4.0f, 0.0f}},
        {{4.0f, 0.0f, -4.0f}, {0.0f, 1.0f, 0.0f}, {4.0f, 4.0f}},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}
} // namespace YRender
