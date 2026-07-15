#include "Assets/MeshFactory.h"

#include "Core/Platform.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

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
    bool normalized = false;
    std::string type;
};

struct GltfBufferView
{
    int buffer = 0;
    int byteOffset = 0;
    int byteLength = 0;
    int byteStride = 0;
};

struct GltfNode
{
    int mesh = -1;
    int skin = -1;
    std::vector<int> children;
    DirectX::XMMATRIX local = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
    bool hasParent = false;
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

bool JsonBoolAfter(const std::string& text, const std::string& key, size_t start = 0, bool fallback = false)
{
    const size_t keyPos = text.find(key, start);
    if (keyPos == std::string::npos)
    {
        return fallback;
    }
    const size_t colon = text.find(':', keyPos);
    const size_t value = text.find_first_not_of(" \t\r\n", colon + 1);
    if (value == std::string::npos)
    {
        return fallback;
    }
    if (text.compare(value, 4, "true") == 0)
    {
        return true;
    }
    if (text.compare(value, 5, "false") == 0)
    {
        return false;
    }
    return fallback;
}

float JsonFloatAfter(const std::string& text, const std::string& key, size_t start = 0, float fallback = 0.0f)
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
    return std::stof(text.substr(value));
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

bool JsonFloatArrayAfter(const std::string& text, const std::string& key, float* values, size_t count)
{
    const size_t keyPos = text.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const size_t open = text.find('[', keyPos);
    const size_t close = text.find(']', open);
    if (open == std::string::npos || close == std::string::npos)
    {
        return false;
    }

    std::stringstream stream(text.substr(open + 1, close - open - 1));
    for (size_t i = 0; i < count; ++i)
    {
        stream >> values[i];
        if (!stream)
        {
            return false;
        }
        if (i + 1 < count)
        {
            char comma = 0;
            stream >> comma;
        }
    }
    return true;
}

std::vector<float> JsonFloatArrayValuesAfter(const std::string& text, const std::string& key)
{
    std::vector<float> values;
    const size_t keyPos = text.find(key);
    if (keyPos == std::string::npos)
    {
        return values;
    }

    const size_t open = text.find('[', keyPos);
    const size_t close = text.find(']', open);
    if (open == std::string::npos || close == std::string::npos)
    {
        return values;
    }

    std::stringstream stream(text.substr(open + 1, close - open - 1));
    while (stream)
    {
        float value = 0.0f;
        stream >> value;
        if (stream)
        {
            values.push_back(value);
        }
        char comma = 0;
        stream >> comma;
    }
    return values;
}

std::vector<int> JsonIntArrayAfter(const std::string& text, const std::string& key)
{
    std::vector<int> values;
    const size_t keyPos = text.find(key);
    if (keyPos == std::string::npos)
    {
        return values;
    }

    const size_t open = text.find('[', keyPos);
    const size_t close = text.find(']', open);
    if (open == std::string::npos || close == std::string::npos)
    {
        return values;
    }

    std::stringstream stream(text.substr(open + 1, close - open - 1));
    while (stream)
    {
        int value = 0;
        stream >> value;
        if (stream)
        {
            values.push_back(value);
        }
        char comma = 0;
        stream >> comma;
    }
    return values;
}

size_t FindMatchingBrace(const std::string& text, size_t objectStart);

std::optional<std::string> JsonArrayObject(const std::string& text, const std::string& arrayKey, int index)
{
    size_t keyPos = std::string::npos;
    int objectDepth = 0;
    int arrayDepth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '"')
            {
                inString = false;
            }
            continue;
        }

        if (c == '"')
        {
            if (objectDepth == 1 && arrayDepth == 0 && text.compare(i, arrayKey.size(), arrayKey) == 0)
            {
                keyPos = i;
                break;
            }
            inString = true;
        }
        else if (c == '{')
        {
            ++objectDepth;
        }
        else if (c == '}')
        {
            --objectDepth;
        }
        else if (c == '[')
        {
            ++arrayDepth;
        }
        else if (c == ']')
        {
            --arrayDepth;
        }
    }

    if (keyPos == std::string::npos)
    {
        return std::nullopt;
    }

    const size_t arrayStart = text.find('[', keyPos);
    if (arrayStart == std::string::npos)
    {
        return std::nullopt;
    }

    int current = 0;
    int depth = 0;
    inString = false;
    escaped = false;
    for (size_t i = arrayStart; i < text.size(); ++i)
    {
        const char c = text[i];
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '"')
            {
                inString = false;
            }
            continue;
        }

        if (c == '"')
        {
            inString = true;
        }
        else if (c == '[')
        {
            ++depth;
        }
        else if (c == ']')
        {
            --depth;
            if (depth == 0)
            {
                break;
            }
        }
        else if (c == '{' && depth == 1)
        {
            const size_t objectEnd = FindMatchingBrace(text, i);
            if (objectEnd == std::string::npos)
            {
                return std::nullopt;
            }

            if (current == index)
            {
                return text.substr(i, objectEnd - i + 1);
            }

            ++current;
            i = objectEnd;
        }
    }
    return std::nullopt;
}

std::vector<std::string> JsonObjectArrayAfter(const std::string& text, const std::string& arrayKey)
{
    std::vector<std::string> objects;
    const size_t keyPos = text.find(arrayKey);
    if (keyPos == std::string::npos)
    {
        return objects;
    }

    const size_t arrayStart = text.find('[', keyPos);
    if (arrayStart == std::string::npos)
    {
        return objects;
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = arrayStart; i < text.size(); ++i)
    {
        const char c = text[i];
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '"')
            {
                inString = false;
            }
            continue;
        }

        if (c == '"')
        {
            inString = true;
        }
        else if (c == '[')
        {
            ++depth;
        }
        else if (c == ']')
        {
            --depth;
            if (depth == 0)
            {
                break;
            }
        }
        else if (c == '{' && depth == 1)
        {
            const size_t objectEnd = FindMatchingBrace(text, i);
            if (objectEnd == std::string::npos)
            {
                break;
            }
            objects.push_back(text.substr(i, objectEnd - i + 1));
            i = objectEnd;
        }
    }
    return objects;
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
    accessor.normalized = JsonBoolAfter(*object, "\"normalized\"", 0, false);
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
    view.byteLength = JsonIntAfter(*object, "\"byteLength\"", 0, 0);
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

bool BufferRangeAvailable(const std::vector<uint8_t>& bytes, size_t offset, size_t byteCount)
{
    return offset <= bytes.size() && byteCount <= bytes.size() - offset;
}

uint32_t ReadLe32(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset + sizeof(uint32_t) > bytes.size())
    {
        return 0;
    }

    uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

bool LoadGlb(const std::wstring& path, std::string& outJson, std::vector<uint8_t>& outBinary)
{
    const std::vector<uint8_t> bytes = ReadBinaryFile(path);
    if (bytes.size() < 20 || ReadLe32(bytes, 0) != 0x46546C67u || ReadLe32(bytes, 4) != 2)
    {
        return false;
    }

    size_t offset = 12;
    while (offset + 8 <= bytes.size())
    {
        const uint32_t chunkLength = ReadLe32(bytes, offset);
        const uint32_t chunkType = ReadLe32(bytes, offset + 4);
        offset += 8;
        if (offset + chunkLength > bytes.size())
        {
            return false;
        }

        if (chunkType == 0x4E4F534Au)
        {
            outJson.assign(reinterpret_cast<const char*>(bytes.data() + offset), chunkLength);
            while (!outJson.empty() && (outJson.back() == '\0' || outJson.back() == ' '))
            {
                outJson.pop_back();
            }
        }
        else if (chunkType == 0x004E4942u)
        {
            outBinary.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
        }

        offset += chunkLength;
    }

    return !outJson.empty() && !outBinary.empty();
}

size_t FindMatchingBrace(const std::string& text, size_t objectStart)
{
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = objectStart; i < text.size(); ++i)
    {
        const char c = text[i];
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '"')
            {
                inString = false;
            }
            continue;
        }

        if (c == '"')
        {
            inString = true;
        }
        else if (c == '{')
        {
            ++depth;
        }
        else if (c == '}')
        {
            --depth;
            if (depth == 0)
            {
                return i;
            }
        }
    }
    return std::string::npos;
}

std::optional<std::string> ObjectContaining(const std::string& text, size_t position)
{
    size_t search = position;
    while (search != std::string::npos)
    {
        const size_t candidate = text.rfind('{', search);
        if (candidate == std::string::npos)
        {
            return std::nullopt;
        }

        const size_t end = FindMatchingBrace(text, candidate);
        if (end != std::string::npos && candidate <= position && position <= end)
        {
            return text.substr(candidate, end - candidate + 1);
        }

        if (candidate == 0)
        {
            break;
        }
        search = candidate - 1;
    }
    return std::nullopt;
}

std::vector<std::string> GltfPrimitiveObjects(const std::string& json)
{
    std::vector<std::string> primitives;
    size_t pos = 0;
    while ((pos = json.find("\"attributes\"", pos)) != std::string::npos)
    {
        if (std::optional<std::string> primitive = ObjectContaining(json, pos))
        {
            if (primitive->find("\"POSITION\"") != std::string::npos)
            {
                primitives.push_back(*primitive);
            }
        }
        pos += 12;
    }
    return primitives;
}

DirectX::XMMATRIX GltfMatrixFromValues(const std::vector<float>& values)
{
    if (values.size() != 16)
    {
        return DirectX::XMMatrixIdentity();
    }

    DirectX::XMFLOAT4X4 matrix{
        values[0], values[1], values[2], values[3],
        values[4], values[5], values[6], values[7],
        values[8], values[9], values[10], values[11],
        values[12], values[13], values[14], values[15]};
    return DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&matrix));
}

DirectX::XMMATRIX ParseNodeLocalMatrix(const std::string& node)
{
    const std::vector<float> matrixValues = JsonFloatArrayValuesAfter(node, "\"matrix\"");
    if (matrixValues.size() == 16)
    {
        return GltfMatrixFromValues(matrixValues);
    }

    DirectX::XMFLOAT3 translation{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};

    const std::vector<float> translationValues = JsonFloatArrayValuesAfter(node, "\"translation\"");
    if (translationValues.size() >= 3)
    {
        translation = DirectX::XMFLOAT3(translationValues[0], translationValues[1], translationValues[2]);
    }

    const std::vector<float> rotationValues = JsonFloatArrayValuesAfter(node, "\"rotation\"");
    if (rotationValues.size() >= 4)
    {
        rotation = DirectX::XMFLOAT4(rotationValues[0], rotationValues[1], rotationValues[2], rotationValues[3]);
    }

    const std::vector<float> scaleValues = JsonFloatArrayValuesAfter(node, "\"scale\"");
    if (scaleValues.size() >= 3)
    {
        scale = DirectX::XMFLOAT3(scaleValues[0], scaleValues[1], scaleValues[2]);
    }

    return DirectX::XMMatrixScaling(scale.x, scale.y, scale.z) *
        DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation)) *
        DirectX::XMMatrixTranslation(translation.x, translation.y, translation.z);
}

void ComputeNodeWorld(std::vector<GltfNode>& nodes, int nodeIndex, const DirectX::XMMATRIX& parentWorld)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size()))
    {
        return;
    }

    GltfNode& node = nodes[static_cast<size_t>(nodeIndex)];
    node.world = node.local * parentWorld;
    for (int child : node.children)
    {
        ComputeNodeWorld(nodes, child, node.world);
    }
}

std::vector<GltfNode> ParseGltfNodes(const std::string& json)
{
    std::vector<GltfNode> nodes;
    for (int i = 0;; ++i)
    {
        const std::optional<std::string> nodeObject = JsonArrayObject(json, "\"nodes\"", i);
        if (!nodeObject)
        {
            break;
        }

        GltfNode node;
        node.mesh = JsonIntAfter(*nodeObject, "\"mesh\"", 0, -1);
        node.skin = JsonIntAfter(*nodeObject, "\"skin\"", 0, -1);
        node.children = JsonIntArrayAfter(*nodeObject, "\"children\"");
        node.local = ParseNodeLocalMatrix(*nodeObject);
        nodes.push_back(node);
    }

    for (GltfNode& node : nodes)
    {
        for (int child : node.children)
        {
            if (child >= 0 && child < static_cast<int>(nodes.size()))
            {
                nodes[static_cast<size_t>(child)].hasParent = true;
            }
        }
    }

    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
    {
        if (!nodes[static_cast<size_t>(i)].hasParent)
        {
            ComputeNodeWorld(nodes, i, DirectX::XMMatrixIdentity());
        }
    }
    return nodes;
}

std::vector<std::string> MeshPrimitiveObjects(const std::string& json, int meshIndex)
{
    const std::optional<std::string> meshObject = JsonArrayObject(json, "\"meshes\"", meshIndex);
    if (!meshObject)
    {
        return {};
    }
    return JsonObjectArrayAfter(*meshObject, "\"primitives\"");
}

std::vector<int> ParseTextureSources(const std::string& json)
{
    std::vector<int> sources;
    for (int i = 0;; ++i)
    {
        const std::optional<std::string> texture = JsonArrayObject(json, "\"textures\"", i);
        if (!texture)
        {
            break;
        }
        sources.push_back(JsonIntAfter(*texture, "\"source\"", 0, -1));
    }
    return sources;
}

void ParseEmbeddedImages(const std::string& json, const std::vector<uint8_t>& bytes, Mesh& outMesh)
{
    for (int i = 0;; ++i)
    {
        const std::optional<std::string> image = JsonArrayObject(json, "\"images\"", i);
        if (!image)
        {
            break;
        }

        const int bufferViewIndex = JsonIntAfter(*image, "\"bufferView\"", 0, -1);
        EmbeddedImage embedded;
        embedded.name = JsonStringAfter(*image, "\"name\"");
        embedded.mimeType = JsonStringAfter(*image, "\"mimeType\"");
        if (bufferViewIndex < 0)
        {
            outMesh.embeddedImages.push_back(std::move(embedded));
            continue;
        }

        const GltfBufferView view = ParseBufferView(json, bufferViewIndex);
        if (view.byteLength <= 0 || !BufferRangeAvailable(bytes, static_cast<size_t>(view.byteOffset), static_cast<size_t>(view.byteLength)))
        {
            outMesh.embeddedImages.push_back(std::move(embedded));
            continue;
        }

        embedded.bytes.assign(bytes.begin() + view.byteOffset, bytes.begin() + view.byteOffset + view.byteLength);
        outMesh.embeddedImages.push_back(std::move(embedded));
    }
}

void ParseImportedMaterials(const std::string& json, Mesh& outMesh)
{
    const std::vector<int> textureSources = ParseTextureSources(json);
    for (int i = 0;; ++i)
    {
        const std::optional<std::string> material = JsonArrayObject(json, "\"materials\"", i);
        if (!material)
        {
            break;
        }

        ImportedMaterial imported;
        imported.name = JsonStringAfter(*material, "\"name\"");
        float color[4]{1.0f, 1.0f, 1.0f, 1.0f};
        if (JsonFloatArrayAfter(*material, "\"baseColorFactor\"", color, 4))
        {
            imported.baseColor = DirectX::XMFLOAT4(color[0], color[1], color[2], color[3]);
        }

        const size_t textureKey = material->find("\"baseColorTexture\"");
        if (textureKey != std::string::npos)
        {
            const int textureIndex = JsonIntAfter(*material, "\"index\"", textureKey, -1);
            if (textureIndex >= 0 && textureIndex < static_cast<int>(textureSources.size()))
            {
                imported.baseColorImage = textureSources[static_cast<size_t>(textureIndex)];
            }
        }

        const std::string alphaMode = JsonStringAfter(*material, "\"alphaMode\"");
        imported.alphaMode = alphaMode.empty() ? "OPAQUE" : alphaMode;
        imported.alphaBlend = alphaMode == "BLEND";
        imported.alphaMask = alphaMode == "MASK";
        imported.unlit = material->find("\"KHR_materials_unlit\"") != std::string::npos ||
            material->find("\"VRMC_materials_mtoon\"") != std::string::npos;
        imported.alphaCutoff = JsonFloatAfter(*material, "\"alphaCutoff\"", 0, 0.5f);
        if (imported.alphaMask)
        {
            imported.alphaCutoff = std::min(imported.alphaCutoff, 0.08f);
        }
        outMesh.importedMaterials.push_back(imported);
    }
}

bool AppendGltfPrimitive(
    const std::string& json,
    const std::vector<uint8_t>& bytes,
    const std::string& primitive,
    const DirectX::XMMATRIX& nodeWorld,
    Mesh& outMesh)
{
    const int positionAccessorIndex = JsonIntAfter(primitive, "\"POSITION\"", 0, -1);
    if (positionAccessorIndex < 0)
    {
        return false;
    }

    const int normalAccessorIndex = JsonIntAfter(primitive, "\"NORMAL\"", 0, -1);
    const int uvAccessorIndex = JsonIntAfter(primitive, "\"TEXCOORD_0\"", 0, -1);
    const int indicesAccessorIndex = JsonIntAfter(primitive, "\"indices\"", 0, -1);
    const int materialIndex = JsonIntAfter(primitive, "\"material\"", 0, -1);
    DirectX::XMFLOAT4 primitiveColor{1.0f, 1.0f, 1.0f, 1.0f};
    if (materialIndex >= 0 && materialIndex < static_cast<int>(outMesh.importedMaterials.size()))
    {
        primitiveColor = outMesh.importedMaterials[static_cast<size_t>(materialIndex)].baseColor;
    }
    else if (materialIndex >= 0)
    {
        if (const std::optional<std::string> material = JsonArrayObject(json, "\"materials\"", materialIndex))
        {
            float values[4]{1.0f, 1.0f, 1.0f, 1.0f};
            if (JsonFloatArrayAfter(*material, "\"baseColorFactor\"", values, 4))
            {
                primitiveColor = DirectX::XMFLOAT4(values[0], values[1], values[2], values[3]);
            }
        }
    }

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

    const bool hasNormals = normalAccessor.componentType == 5126 && normalAccessor.type == "VEC3";
    const bool hasTexcoords = uvAccessor.componentType == 5126 && uvAccessor.type == "VEC2";
    const int positionStride = positionView.byteStride > 0 ? positionView.byteStride : static_cast<int>(sizeof(float) * 3);
    const int normalStride = normalView.byteStride > 0 ? normalView.byteStride : static_cast<int>(sizeof(float) * 3);
    const int uvStride = uvView.byteStride > 0 ? uvView.byteStride : static_cast<int>(sizeof(float) * 2);
    const uint32_t baseVertex = static_cast<uint32_t>(outMesh.vertices.size());
    const uint32_t indexStart = static_cast<uint32_t>(outMesh.indices.size());
    outMesh.vertices.reserve(outMesh.vertices.size() + static_cast<size_t>(positionAccessor.count));

    const DirectX::XMMATRIX normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, nodeWorld));
    for (int i = 0; i < positionAccessor.count; ++i)
    {
        const size_t positionOffset = static_cast<size_t>(positionView.byteOffset + positionAccessor.byteOffset + i * positionStride);
        if (!BufferRangeAvailable(bytes, positionOffset, sizeof(float) * 3))
        {
            return false;
        }
        const float* position = reinterpret_cast<const float*>(bytes.data() + positionOffset);

        Vertex vertex{};
        DirectX::XMStoreFloat3(
            &vertex.position,
            DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(position[0], position[1], position[2], 1.0f), nodeWorld));
        vertex.normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
        vertex.uv = DirectX::XMFLOAT2(0.0f, 0.0f);
        vertex.color = primitiveColor;

        if (hasNormals && i < normalAccessor.count)
        {
            const size_t normalOffset = static_cast<size_t>(normalView.byteOffset + normalAccessor.byteOffset + i * normalStride);
            if (BufferRangeAvailable(bytes, normalOffset, sizeof(float) * 3))
            {
                const float* normal = reinterpret_cast<const float*>(bytes.data() + normalOffset);
                const DirectX::XMVECTOR transformedNormal = DirectX::XMVector3Normalize(
                    DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(normal[0], normal[1], normal[2], 0.0f), normalMatrix));
                DirectX::XMStoreFloat3(&vertex.normal, transformedNormal);
            }
        }

        if (hasTexcoords && i < uvAccessor.count)
        {
            const size_t uvOffset = static_cast<size_t>(uvView.byteOffset + uvAccessor.byteOffset + i * uvStride);
            if (BufferRangeAvailable(bytes, uvOffset, sizeof(float) * 2))
            {
                const float* uv = reinterpret_cast<const float*>(bytes.data() + uvOffset);
                vertex.uv = DirectX::XMFLOAT2(uv[0], uv[1]);
            }
        }

        outMesh.vertices.push_back(vertex);
    }

    outMesh.hasNormals = outMesh.hasNormals || hasNormals;
    outMesh.hasTexcoords = outMesh.hasTexcoords || hasTexcoords;

    if (indicesAccessorIndex >= 0)
    {
        const GltfAccessor indexAccessor = ParseAccessor(json, indicesAccessorIndex);
        const GltfBufferView indexView = ParseBufferView(json, indexAccessor.bufferView);
        outMesh.indices.reserve(outMesh.indices.size() + static_cast<size_t>(indexAccessor.count));
        const size_t indexBaseOffset = static_cast<size_t>(indexView.byteOffset + indexAccessor.byteOffset);
        for (int i = 0; i < indexAccessor.count; ++i)
        {
            uint32_t index = 0;
            if (indexAccessor.componentType == 5121)
            {
                const size_t offset = indexBaseOffset + static_cast<size_t>(i) * sizeof(uint8_t);
                if (!BufferRangeAvailable(bytes, offset, sizeof(uint8_t)))
                {
                    return false;
                }
                index = bytes[offset];
            }
            else if (indexAccessor.componentType == 5123)
            {
                const size_t offset = indexBaseOffset + static_cast<size_t>(i) * sizeof(uint16_t);
                if (!BufferRangeAvailable(bytes, offset, sizeof(uint16_t)))
                {
                    return false;
                }
                uint16_t value = 0;
                std::memcpy(&value, bytes.data() + offset, sizeof(value));
                index = value;
            }
            else if (indexAccessor.componentType == 5125)
            {
                const size_t offset = indexBaseOffset + static_cast<size_t>(i) * sizeof(uint32_t);
                if (!BufferRangeAvailable(bytes, offset, sizeof(uint32_t)))
                {
                    return false;
                }
                std::memcpy(&index, bytes.data() + offset, sizeof(index));
            }
            else
            {
                return false;
            }
            outMesh.indices.push_back(baseVertex + index);
        }
    }
    else
    {
        outMesh.indices.reserve(outMesh.indices.size() + static_cast<size_t>(positionAccessor.count));
        for (uint32_t i = 0; i < static_cast<uint32_t>(positionAccessor.count); ++i)
        {
            outMesh.indices.push_back(baseVertex + i);
        }
    }

    SubMesh submesh;
    submesh.indexStart = indexStart;
    submesh.indexCount = static_cast<uint32_t>(outMesh.indices.size()) - indexStart;
    submesh.materialIndex = materialIndex;
    if (submesh.indexCount > 0)
    {
        outMesh.submeshes.push_back(submesh);
    }

    return true;
}

bool LoadGltfFromData(const std::wstring& path, const std::string& json, const std::vector<uint8_t>& bytes, Mesh& outMesh)
{
    if (json.empty() || bytes.empty())
    {
        return false;
    }

    outMesh = {};
    outMesh.sourcePath = WideToUtf8(path);
    outMesh.displayName = std::filesystem::path(path).filename().string();
    ParseEmbeddedImages(json, bytes, outMesh);
    ParseImportedMaterials(json, outMesh);

    const std::vector<GltfNode> nodes = ParseGltfNodes(json);
    outMesh.gltfNodeCount = static_cast<int>(nodes.size());
    for (const GltfNode& node : nodes)
    {
        if (node.mesh >= 0)
        {
            ++outMesh.gltfMeshNodeCount;
        }
    }
    for (int i = 0;; ++i)
    {
        if (!JsonArrayObject(json, "\"skins\"", i))
        {
            break;
        }
        ++outMesh.gltfSkinCount;
    }
    if (outMesh.gltfSkinCount > 0)
    {
        outMesh.importNotes = "glTF skin data detected. Day2.5 imports this model as a stable static preview; animation/skinning is intentionally not applied yet.";
    }
    else
    {
        outMesh.importNotes = "Static glTF/GLB mesh import.";
    }

    bool appendedAnyPrimitive = false;

    for (const GltfNode& node : nodes)
    {
        if (node.mesh < 0)
        {
            continue;
        }

        const std::vector<std::string> primitives = MeshPrimitiveObjects(json, node.mesh);
        for (const std::string& primitive : primitives)
        {
            appendedAnyPrimitive = AppendGltfPrimitive(json, bytes, primitive, node.world, outMesh) || appendedAnyPrimitive;
        }
    }

    if (!appendedAnyPrimitive)
    {
        const std::vector<std::string> primitives = GltfPrimitiveObjects(json);
        if (primitives.empty())
        {
            return false;
        }

        for (const std::string& primitive : primitives)
        {
            appendedAnyPrimitive = AppendGltfPrimitive(json, bytes, primitive, DirectX::XMMatrixIdentity(), outMesh) || appendedAnyPrimitive;
        }
    }

    UpdateMeshBounds(outMesh);
    return appendedAnyPrimitive && !outMesh.vertices.empty() && !outMesh.indices.empty();
}

void GenerateNormals(Mesh& mesh)
{
    for (Vertex& vertex : mesh.vertices)
    {
        vertex.normal = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        Vertex& a = mesh.vertices[mesh.indices[i + 0]];
        Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        const DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&a.position);
        const DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&b.position);
        const DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&c.position);
        const DirectX::XMVECTOR normal = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(p1, p0), DirectX::XMVectorSubtract(p2, p0));
        DirectX::XMFLOAT3 n{};
        DirectX::XMStoreFloat3(&n, normal);
        a.normal.x += n.x;
        a.normal.y += n.y;
        a.normal.z += n.z;
        b.normal.x += n.x;
        b.normal.y += n.y;
        b.normal.z += n.z;
        c.normal.x += n.x;
        c.normal.y += n.y;
        c.normal.z += n.z;
    }

    for (Vertex& vertex : mesh.vertices)
    {
        DirectX::XMVECTOR normal = DirectX::XMLoadFloat3(&vertex.normal);
        if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(normal)) <= 0.000001f)
        {
            normal = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }
        else
        {
            normal = DirectX::XMVector3Normalize(normal);
        }
        DirectX::XMStoreFloat3(&vertex.normal, normal);
    }

    mesh.hasNormals = true;
}

void AppendQuad(Mesh& mesh, const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d)
{
    const uint32_t start = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(a);
    mesh.vertices.push_back(b);
    mesh.vertices.push_back(c);
    mesh.vertices.push_back(d);
    mesh.indices.push_back(start + 0);
    mesh.indices.push_back(start + 1);
    mesh.indices.push_back(start + 2);
    mesh.indices.push_back(start + 0);
    mesh.indices.push_back(start + 2);
    mesh.indices.push_back(start + 3);
}

void AppendBox(Mesh& mesh, DirectX::XMFLOAT3 center, DirectX::XMFLOAT3 halfExtents)
{
    const float x0 = center.x - halfExtents.x;
    const float x1 = center.x + halfExtents.x;
    const float y0 = center.y - halfExtents.y;
    const float y1 = center.y + halfExtents.y;
    const float z0 = center.z - halfExtents.z;
    const float z1 = center.z + halfExtents.z;

    AppendQuad(mesh, {{x0, y0, z0}, {0, 0, -1}, {0, 1}}, {{x0, y1, z0}, {0, 0, -1}, {0, 0}}, {{x1, y1, z0}, {0, 0, -1}, {1, 0}}, {{x1, y0, z0}, {0, 0, -1}, {1, 1}});
    AppendQuad(mesh, {{x1, y0, z1}, {0, 0, 1}, {0, 1}}, {{x1, y1, z1}, {0, 0, 1}, {0, 0}}, {{x0, y1, z1}, {0, 0, 1}, {1, 0}}, {{x0, y0, z1}, {0, 0, 1}, {1, 1}});
    AppendQuad(mesh, {{x0, y0, z1}, {-1, 0, 0}, {0, 1}}, {{x0, y1, z1}, {-1, 0, 0}, {0, 0}}, {{x0, y1, z0}, {-1, 0, 0}, {1, 0}}, {{x0, y0, z0}, {-1, 0, 0}, {1, 1}});
    AppendQuad(mesh, {{x1, y0, z0}, {1, 0, 0}, {0, 1}}, {{x1, y1, z0}, {1, 0, 0}, {0, 0}}, {{x1, y1, z1}, {1, 0, 0}, {1, 0}}, {{x1, y0, z1}, {1, 0, 0}, {1, 1}});
    AppendQuad(mesh, {{x0, y1, z0}, {0, 1, 0}, {0, 1}}, {{x0, y1, z1}, {0, 1, 0}, {0, 0}}, {{x1, y1, z1}, {0, 1, 0}, {1, 0}}, {{x1, y1, z0}, {0, 1, 0}, {1, 1}});
    AppendQuad(mesh, {{x0, y0, z1}, {0, -1, 0}, {0, 1}}, {{x0, y0, z0}, {0, -1, 0}, {0, 0}}, {{x1, y0, z0}, {0, -1, 0}, {1, 0}}, {{x1, y0, z1}, {0, -1, 0}, {1, 1}});
}
} // namespace

bool LoadObj(const std::wstring& path, Mesh& outMesh)
{
    std::ifstream file(path);
    if (!file)
    {
        return false;
    }

    outMesh = {};
    outMesh.sourcePath = WideToUtf8(path);
    outMesh.displayName = std::filesystem::path(path).filename().string();

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
                    outMesh.hasNormals = outMesh.hasNormals || key.normal != 0;
                    outMesh.hasTexcoords = outMesh.hasTexcoords || key.texcoord != 0;
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

    if (!outMesh.hasNormals && !outMesh.vertices.empty() && !outMesh.indices.empty())
    {
        GenerateNormals(outMesh);
    }
    UpdateMeshBounds(outMesh);
    return !outMesh.vertices.empty() && !outMesh.indices.empty();
}

bool LoadGltf(const std::wstring& path, Mesh& outMesh)
{
    std::string json;
    std::vector<uint8_t> bytes;
    const std::filesystem::path fsPath(path);
    const std::wstring extension = fsPath.extension().wstring();
    if (extension == L".glb" || extension == L".GLB")
    {
        if (!LoadGlb(path, json, bytes))
        {
            return false;
        }
    }
    else
    {
        json = ReadTextFile(path);
        if (json.empty())
        {
            return false;
        }

        const std::string uri = JsonStringAfter(json, "\"uri\"");
        if (uri.empty())
        {
            return false;
        }

        const std::filesystem::path binPath = fsPath.parent_path() / std::filesystem::path(uri);
        bytes = ReadBinaryFile(binPath.wstring());
        if (bytes.empty())
        {
            return false;
        }
    }

    if (!LoadGltfFromData(path, json, bytes, outMesh))
    {
        return false;
    }

    if (!outMesh.hasNormals)
    {
        GenerateNormals(outMesh);
    }
    UpdateMeshBounds(outMesh);
    return true;
}

bool LoadStaticMesh(const std::wstring& path, Mesh& outMesh)
{
    const std::filesystem::path fsPath(path);
    const std::wstring extension = fsPath.extension().wstring();
    if (extension == L".obj" || extension == L".OBJ")
    {
        return LoadObj(path, outMesh);
    }
    if (extension == L".gltf" || extension == L".GLTF" || extension == L".glb" || extension == L".GLB")
    {
        return LoadGltf(path, outMesh);
    }
    return false;
}

void UpdateMeshBounds(Mesh& mesh)
{
    if (mesh.vertices.empty())
    {
        mesh.boundsMin = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
        mesh.boundsMax = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
        return;
    }

    DirectX::XMFLOAT3 minValue{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    DirectX::XMFLOAT3 maxValue{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
    };

    for (const Vertex& vertex : mesh.vertices)
    {
        minValue.x = std::min(minValue.x, vertex.position.x);
        minValue.y = std::min(minValue.y, vertex.position.y);
        minValue.z = std::min(minValue.z, vertex.position.z);
        maxValue.x = std::max(maxValue.x, vertex.position.x);
        maxValue.y = std::max(maxValue.y, vertex.position.y);
        maxValue.z = std::max(maxValue.z, vertex.position.z);
    }

    mesh.boundsMin = minValue;
    mesh.boundsMax = maxValue;
}

void NormalizeMeshForPreview(Mesh& mesh, float targetHeight)
{
    UpdateMeshBounds(mesh);
    const float width = mesh.boundsMax.x - mesh.boundsMin.x;
    const float height = mesh.boundsMax.y - mesh.boundsMin.y;
    const float depth = mesh.boundsMax.z - mesh.boundsMin.z;
    const float longest = std::max({width, height, depth, 0.0001f});
    const float scale = targetHeight / longest;
    const DirectX::XMFLOAT3 center{
        (mesh.boundsMin.x + mesh.boundsMax.x) * 0.5f,
        mesh.boundsMin.y,
        (mesh.boundsMin.z + mesh.boundsMax.z) * 0.5f,
    };

    for (Vertex& vertex : mesh.vertices)
    {
        vertex.position.x = (vertex.position.x - center.x) * scale;
        vertex.position.y = (vertex.position.y - center.y) * scale;
        vertex.position.z = (vertex.position.z - center.z) * scale;
    }

    mesh.normalizedForPreview = true;
    UpdateMeshBounds(mesh);
}

Mesh CreateCubeMesh()
{
    Mesh mesh;
    mesh.displayName = "Fallback Cube";
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
    mesh.hasNormals = true;
    mesh.hasTexcoords = true;
    UpdateMeshBounds(mesh);
    return mesh;
}

Mesh CreateDogMesh()
{
    Mesh mesh;
    mesh.displayName = "Procedural Shader Dog";
    AppendBox(mesh, {0.0f, 0.15f, 0.15f}, {1.05f, 0.42f, 0.42f});      // body
    AppendBox(mesh, {0.0f, 0.55f, -0.75f}, {0.50f, 0.42f, 0.42f});     // head
    AppendBox(mesh, {0.0f, 0.42f, -1.18f}, {0.34f, 0.18f, 0.26f});     // snout
    AppendBox(mesh, {-0.32f, 1.00f, -0.76f}, {0.14f, 0.34f, 0.12f});   // left ear
    AppendBox(mesh, {0.32f, 1.00f, -0.76f}, {0.14f, 0.34f, 0.12f});    // right ear
    AppendBox(mesh, {-0.66f, -0.55f, -0.28f}, {0.18f, 0.42f, 0.18f});  // front left leg
    AppendBox(mesh, {0.66f, -0.55f, -0.28f}, {0.18f, 0.42f, 0.18f});   // front right leg
    AppendBox(mesh, {-0.66f, -0.55f, 0.62f}, {0.18f, 0.42f, 0.18f});   // back left leg
    AppendBox(mesh, {0.66f, -0.55f, 0.62f}, {0.18f, 0.42f, 0.18f});    // back right leg
    AppendBox(mesh, {0.0f, -0.90f, -0.28f}, {0.22f, 0.10f, 0.22f});    // front paws
    AppendBox(mesh, {0.0f, -0.90f, 0.62f}, {0.22f, 0.10f, 0.22f});     // back paws
    AppendBox(mesh, {0.0f, 0.42f, 1.02f}, {0.14f, 0.14f, 0.46f});      // tail
    mesh.hasNormals = true;
    mesh.hasTexcoords = true;
    UpdateMeshBounds(mesh);
    return mesh;
}

Mesh CreatePlaneMesh()
{
    Mesh mesh;
    mesh.displayName = "Showcase Platform";

    // A shallow round platform reads more like a presentation stage than a
    // large, empty square plane. The slightly smaller lower ring gives the
    // edge a soft bevel when the camera sees the platform from above.
    constexpr int segments = 48;
    constexpr float topRadius = 3.65f;
    constexpr float bottomRadius = 3.48f;
    constexpr float depth = 0.14f;
    constexpr float pi = 3.14159265359f;

    mesh.vertices.reserve(1 + segments * 3);
    mesh.vertices.push_back(Vertex{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}, {0.86f, 0.92f, 0.93f, 1.0f}});
    for (int i = 0; i < segments; ++i)
    {
        const float angle = static_cast<float>(i) * 2.0f * pi / static_cast<float>(segments);
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        mesh.vertices.push_back(Vertex{{topRadius * x, 0.0f, topRadius * z}, {0.0f, 1.0f, 0.0f}, {0.5f + x * 0.5f, 0.5f + z * 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}});
    }
    for (int i = 0; i < segments; ++i)
    {
        const float angle = static_cast<float>(i) * 2.0f * pi / static_cast<float>(segments);
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        mesh.vertices.push_back(Vertex{{topRadius * x, -depth * 0.45f, topRadius * z}, {x, 0.0f, z}, {static_cast<float>(i) / segments, 1.0f}, {0.82f, 0.90f, 0.92f, 1.0f}});
    }
    for (int i = 0; i < segments; ++i)
    {
        const float angle = static_cast<float>(i) * 2.0f * pi / static_cast<float>(segments);
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        mesh.vertices.push_back(Vertex{{bottomRadius * x, -depth, bottomRadius * z}, {x, 0.0f, z}, {static_cast<float>(i) / segments, 0.0f}, {0.68f, 0.80f, 0.83f, 1.0f}});
    }

    const uint32_t topCenter = 0;
    const uint32_t topRing = 1;
    const uint32_t sideTopRing = topRing + segments;
    const uint32_t sideBottomRing = sideTopRing + segments;
    mesh.indices.reserve(static_cast<size_t>(segments) * 12);
    for (int i = 0; i < segments; ++i)
    {
        const uint32_t current = topRing + static_cast<uint32_t>(i);
        const uint32_t next = topRing + static_cast<uint32_t>((i + 1) % segments);
        mesh.indices.insert(mesh.indices.end(), {topCenter, next, current});

        const uint32_t sideCurrent = sideTopRing + static_cast<uint32_t>(i);
        const uint32_t sideNext = sideTopRing + static_cast<uint32_t>((i + 1) % segments);
        const uint32_t bottomCurrent = sideBottomRing + static_cast<uint32_t>(i);
        const uint32_t bottomNext = sideBottomRing + static_cast<uint32_t>((i + 1) % segments);
        mesh.indices.insert(mesh.indices.end(), {sideCurrent, sideNext, bottomNext, sideCurrent, bottomNext, bottomCurrent});
    }
    mesh.hasNormals = true;
    mesh.hasTexcoords = true;
    UpdateMeshBounds(mesh);
    return mesh;
}
} // namespace YRender
