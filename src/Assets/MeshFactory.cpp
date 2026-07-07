#include "Assets/MeshFactory.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
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

namespace
{
struct GltfAccessor
{
    int bufferView = -1;
    int componentType = 0;
    int count = 0;
    int byteOffset = 0;
    std::string type;
};

struct GltfBufferView
{
    int buffer = 0;
    int byteOffset = 0;
    int byteStride = 0;
};

std::string ReadTextFile(const std::wstring& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

std::vector<uint8_t> ReadBinaryFile(const std::wstring& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return {};
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

int JsonIntAfter(const std::string& text, const std::string& key, size_t start = 0, int fallback = 0)
{
    const size_t keyPos = text.find(key, start);
    if (keyPos == std::string::npos)
    {
        return fallback;
    }
    const size_t colon = text.find(':', keyPos);
    const size_t value = text.find_first_of("-0123456789", colon);
    if (value == std::string::npos)
    {
        return fallback;
    }
    return std::stoi(text.substr(value));
}

std::string JsonStringAfter(const std::string& text, const std::string& key, size_t start = 0)
{
    const size_t keyPos = text.find(key, start);
    if (keyPos == std::string::npos)
    {
        return {};
    }
    const size_t colon = text.find(':', keyPos);
    const size_t firstQuote = text.find('"', colon + 1);
    const size_t secondQuote = text.find('"', firstQuote + 1);
    if (firstQuote == std::string::npos || secondQuote == std::string::npos)
    {
        return {};
    }
    return text.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

std::optional<std::string> JsonArrayObject(const std::string& text, const std::string& arrayKey, int index)
{
    const size_t keyPos = text.find(arrayKey);
    if (keyPos == std::string::npos)
    {
        return std::nullopt;
    }
    size_t pos = text.find('[', keyPos);
    for (int current = 0; pos != std::string::npos; ++current)
    {
        const size_t objectStart = text.find('{', pos);
        if (objectStart == std::string::npos)
        {
            return std::nullopt;
        }

        int depth = 0;
        for (size_t i = objectStart; i < text.size(); ++i)
        {
            if (text[i] == '{')
            {
                ++depth;
            }
            else if (text[i] == '}')
            {
                --depth;
                if (depth == 0)
                {
                    if (current == index)
                    {
                        return text.substr(objectStart, i - objectStart + 1);
                    }
                    pos = i + 1;
                    break;
                }
            }
        }
    }
    return std::nullopt;
}

GltfAccessor ParseAccessor(const std::string& json, int index)
{
    const std::optional<std::string> object = JsonArrayObject(json, "\"accessors\"", index);
    if (!object)
    {
        return {};
    }
    GltfAccessor accessor;
    accessor.bufferView = JsonIntAfter(*object, "\"bufferView\"");
    accessor.componentType = JsonIntAfter(*object, "\"componentType\"");
    accessor.count = JsonIntAfter(*object, "\"count\"");
    accessor.byteOffset = JsonIntAfter(*object, "\"byteOffset\"", 0, 0);
    accessor.type = JsonStringAfter(*object, "\"type\"");
    return accessor;
}

GltfBufferView ParseBufferView(const std::string& json, int index)
{
    const std::optional<std::string> object = JsonArrayObject(json, "\"bufferViews\"", index);
    if (!object)
    {
        return {};
    }
    GltfBufferView view;
    view.buffer = JsonIntAfter(*object, "\"buffer\"");
    view.byteOffset = JsonIntAfter(*object, "\"byteOffset\"", 0, 0);
    view.byteStride = JsonIntAfter(*object, "\"byteStride\"", 0, 0);
    return view;
}

int AttributeAccessorIndex(const std::string& json, const std::string& attribute)
{
    const size_t pos = json.find("\"" + attribute + "\"");
    if (pos == std::string::npos)
    {
        return -1;
    }
    return JsonIntAfter(json, "\"" + attribute + "\"", pos, -1);
}

template <typename T>
const T* BufferAt(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset + sizeof(T) > bytes.size())
    {
        return nullptr;
    }
    return reinterpret_cast<const T*>(bytes.data() + offset);
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

bool LoadGltf(const std::wstring& path, Mesh& outMesh)
{
    const std::string json = ReadTextFile(path);
    if (json.empty())
    {
        return false;
    }

    const std::string uri = JsonStringAfter(json, "\"uri\"");
    if (uri.empty())
    {
        return false;
    }

    const std::filesystem::path binPath = std::filesystem::path(path).parent_path() / std::filesystem::path(uri);
    const std::vector<uint8_t> bytes = ReadBinaryFile(binPath.wstring());
    if (bytes.empty())
    {
        return false;
    }

    const int positionAccessorIndex = AttributeAccessorIndex(json, "POSITION");
    if (positionAccessorIndex < 0)
    {
        return false;
    }
    const int normalAccessorIndex = AttributeAccessorIndex(json, "NORMAL");
    const int uvAccessorIndex = AttributeAccessorIndex(json, "TEXCOORD_0");
    const int indicesAccessorIndex = JsonIntAfter(json, "\"indices\"", 0, -1);

    const GltfAccessor positionAccessor = ParseAccessor(json, positionAccessorIndex);
    const GltfAccessor normalAccessor = normalAccessorIndex >= 0 ? ParseAccessor(json, normalAccessorIndex) : GltfAccessor{};
    const GltfAccessor uvAccessor = uvAccessorIndex >= 0 ? ParseAccessor(json, uvAccessorIndex) : GltfAccessor{};
    const GltfBufferView positionView = ParseBufferView(json, positionAccessor.bufferView);
    const GltfBufferView normalView = normalAccessor.bufferView >= 0 ? ParseBufferView(json, normalAccessor.bufferView) : GltfBufferView{};
    const GltfBufferView uvView = uvAccessor.bufferView >= 0 ? ParseBufferView(json, uvAccessor.bufferView) : GltfBufferView{};

    if (positionAccessor.componentType != 5126 || positionAccessor.type != "VEC3" || positionAccessor.count <= 0)
    {
        return false;
    }

    outMesh.vertices.resize(static_cast<size_t>(positionAccessor.count));
    const int positionStride = positionView.byteStride > 0 ? positionView.byteStride : static_cast<int>(sizeof(float) * 3);
    const int normalStride = normalView.byteStride > 0 ? normalView.byteStride : static_cast<int>(sizeof(float) * 3);
    const int uvStride = uvView.byteStride > 0 ? uvView.byteStride : static_cast<int>(sizeof(float) * 2);

    for (int i = 0; i < positionAccessor.count; ++i)
    {
        const size_t positionOffset = static_cast<size_t>(positionView.byteOffset + positionAccessor.byteOffset + i * positionStride);
        const float* position = BufferAt<float>(bytes, positionOffset);
        if (!position)
        {
            return false;
        }
        outMesh.vertices[static_cast<size_t>(i)].position = DirectX::XMFLOAT3(position[0], position[1], position[2]);
        outMesh.vertices[static_cast<size_t>(i)].normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
        outMesh.vertices[static_cast<size_t>(i)].uv = DirectX::XMFLOAT2(0.0f, 0.0f);

        if (normalAccessor.componentType == 5126 && normalAccessor.type == "VEC3" && i < normalAccessor.count)
        {
            const size_t normalOffset = static_cast<size_t>(normalView.byteOffset + normalAccessor.byteOffset + i * normalStride);
            if (const float* normal = BufferAt<float>(bytes, normalOffset))
            {
                outMesh.vertices[static_cast<size_t>(i)].normal = DirectX::XMFLOAT3(normal[0], normal[1], normal[2]);
            }
        }

        if (uvAccessor.componentType == 5126 && uvAccessor.type == "VEC2" && i < uvAccessor.count)
        {
            const size_t uvOffset = static_cast<size_t>(uvView.byteOffset + uvAccessor.byteOffset + i * uvStride);
            if (const float* uv = BufferAt<float>(bytes, uvOffset))
            {
                outMesh.vertices[static_cast<size_t>(i)].uv = DirectX::XMFLOAT2(uv[0], 1.0f - uv[1]);
            }
        }
    }

    if (indicesAccessorIndex >= 0)
    {
        const GltfAccessor indexAccessor = ParseAccessor(json, indicesAccessorIndex);
        const GltfBufferView indexView = ParseBufferView(json, indexAccessor.bufferView);
        outMesh.indices.reserve(static_cast<size_t>(indexAccessor.count));
        for (int i = 0; i < indexAccessor.count; ++i)
        {
            const size_t indexOffset = static_cast<size_t>(indexView.byteOffset + indexAccessor.byteOffset);
            if (indexAccessor.componentType == 5123)
            {
                const uint16_t* value = BufferAt<uint16_t>(bytes, indexOffset + static_cast<size_t>(i) * sizeof(uint16_t));
                if (!value)
                {
                    return false;
                }
                outMesh.indices.push_back(*value);
            }
            else if (indexAccessor.componentType == 5125)
            {
                const uint32_t* value = BufferAt<uint32_t>(bytes, indexOffset + static_cast<size_t>(i) * sizeof(uint32_t));
                if (!value)
                {
                    return false;
                }
                outMesh.indices.push_back(*value);
            }
        }
    }
    else
    {
        outMesh.indices.reserve(outMesh.vertices.size());
        for (uint32_t i = 0; i < outMesh.vertices.size(); ++i)
        {
            outMesh.indices.push_back(i);
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
