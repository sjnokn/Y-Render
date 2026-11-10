#include "Assets/MeshFactory.h"

#include "Core/Log.h"

#include "Core/Platform.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <tinygltf.h>

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
// ---------------------------------------------------------------------------
// glTF loading backed by tinygltf.
//
// Phase 4: all math is now GLM (column-vector). Two latent DX bugs are fixed
// as part of the migration: (1) the node.matrix branch previously transposed
// twice (load row-major into XMFLOAT4X4, then XMMatrixTranspose) which applied
// M^T - now glm::make_mat4 reads the column-major values directly; (2) the
// node world composition was local*parentWorld (row-vector) - now parent*local
// (column-vector).
// ---------------------------------------------------------------------------

struct GltfNode
{
    int mesh = -1;
    int skin = -1;
    std::vector<int> children;
    glm::mat4 local = glm::mat4(1.0f);
    glm::mat4 world = glm::mat4(1.0f);
};

// glm::normalize on a zero-length vector returns NaN and would poison the
// whole frame; preserve the old hand-written Normalize fallback semantics.
glm::vec3 normalizeOr(const glm::vec3& value, const glm::vec3& fallback)
{
    const float lengthSq = glm::dot(value, value);
    return lengthSq > 0.00001f ? value * (1.0f / std::sqrt(lengthSq)) : fallback;
}

const unsigned char* AccessorData(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    size_t& outStride)
{
    if (accessor.bufferView < 0)
    {
        return nullptr;
    }
    if (static_cast<size_t>(accessor.bufferView) >= model.bufferViews.size())
    {
        return nullptr;
    }
    const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= model.buffers.size())
    {
        return nullptr;
    }

    // tinygltf's ByteStride resolves the tightly-packed default (byteStride==0)
    // to componentSize * numComponents and returns -1 for invalid layouts.
    const int stride = accessor.ByteStride(view);
    if (stride <= 0)
    {
        return nullptr;
    }
    outStride = static_cast<size_t>(stride);

    const int componentSize = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
    const int numComponents = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type));
    if (componentSize <= 0 || numComponents <= 0)
    {
        return nullptr;
    }
    const size_t elementSize = static_cast<size_t>(componentSize) * static_cast<size_t>(numComponents);

    const tinygltf::Buffer& buffer = model.buffers[static_cast<size_t>(view.buffer)];
    const size_t offset = static_cast<size_t>(view.byteOffset) + accessor.byteOffset;
    if (accessor.count > 0)
    {
        const size_t needed = offset + outStride * (accessor.count - 1) + elementSize;
        if (needed > buffer.data.size())
        {
            return nullptr;
        }
    }
    return buffer.data.data() + offset;
}

float DecodeComponent(const unsigned char* data, int componentType, bool normalized)
{
    switch (componentType)
    {
    case 5126: // FLOAT
    {
        float value = 0.0f;
        std::memcpy(&value, data, sizeof(float));
        return value;
    }
    case 5120: // BYTE
    {
        int8_t value = 0;
        std::memcpy(&value, data, sizeof(int8_t));
        return normalized ? std::max(static_cast<float>(value) / 127.0f, -1.0f) : static_cast<float>(value);
    }
    case 5121: // UNSIGNED_BYTE
    {
        uint8_t value = 0;
        std::memcpy(&value, data, sizeof(uint8_t));
        return normalized ? static_cast<float>(value) / 255.0f : static_cast<float>(value);
    }
    case 5122: // SHORT
    {
        int16_t value = 0;
        std::memcpy(&value, data, sizeof(int16_t));
        return normalized ? std::max(static_cast<float>(value) / 32767.0f, -1.0f) : static_cast<float>(value);
    }
    case 5123: // UNSIGNED_SHORT
    {
        uint16_t value = 0;
        std::memcpy(&value, data, sizeof(uint16_t));
        return normalized ? static_cast<float>(value) / 65535.0f : static_cast<float>(value);
    }
    case 5125: // UNSIGNED_INT (never normalized in glTF)
    {
        uint32_t value = 0;
        std::memcpy(&value, data, sizeof(uint32_t));
        return static_cast<float>(value);
    }
    default:
        return 0.0f;
    }
}

void ReadAttribute(
    const unsigned char* element,
    int componentType,
    bool normalized,
    int numComponents,
    float* outValues)
{
    const int componentSize = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(componentType));
    for (int c = 0; c < numComponents; ++c)
    {
        outValues[c] = DecodeComponent(
            element + static_cast<size_t>(c) * static_cast<size_t>(componentSize),
            componentType,
            normalized);
    }
}

uint32_t ReadIndex(const unsigned char* data, int componentType)
{
    switch (componentType)
    {
    case 5121: // UNSIGNED_BYTE
    {
        uint8_t value = 0;
        std::memcpy(&value, data, sizeof(uint8_t));
        return static_cast<uint32_t>(value);
    }
    case 5123: // UNSIGNED_SHORT
    {
        uint16_t value = 0;
        std::memcpy(&value, data, sizeof(uint16_t));
        return static_cast<uint32_t>(value);
    }
    case 5125: // UNSIGNED_INT
    {
        uint32_t value = 0;
        std::memcpy(&value, data, sizeof(uint32_t));
        return value;
    }
    default:
        return 0;
    }
}

glm::mat4 NodeLocalMatrix(const tinygltf::Node& node)
{
    // Matrix branch: glTF stores the 16 doubles in column-major order.
    // glm::make_mat4 reads them column-major directly, yielding the true M.
    // (The previous DX code loaded them row-major into XMFLOAT4X4 and then
    // transposed, which net-applied M^T - a latent bug, fixed here.)
    if (node.matrix.size() == 16)
    {
        float values[16];
        for (int i = 0; i < 16; ++i)
        {
            values[i] = static_cast<float>(node.matrix[static_cast<size_t>(i)]);
        }
        return glm::make_mat4(values);
    }

    // TRS branch: column-vector T*Q*S (applies S, then Q, then T to a point).
    glm::vec3 translation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    if (node.translation.size() >= 3)
    {
        translation = glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2]));
    }

    // glTF stores rotation as (x, y, z, w); glm::quat ctor is (w, x, y, z).
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    if (node.rotation.size() >= 4)
    {
        rotation = glm::quat(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]));
    }

    if (node.scale.size() >= 3)
    {
        scale = glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2]));
    }

    return glm::translate(glm::mat4(1.0f), translation) *
        glm::mat4_cast(rotation) *
        glm::scale(glm::mat4(1.0f), scale);
}

void ComputeNodeWorld(std::vector<GltfNode>& nodes, int nodeIndex, const glm::mat4& parentWorld)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size()))
    {
        return;
    }

    GltfNode& node = nodes[static_cast<size_t>(nodeIndex)];
    // Column-vector world = parent * local (was local * parent in DX row-vector).
    node.world = parentWorld * node.local;
    for (int child : node.children)
    {
        ComputeNodeWorld(nodes, child, node.world);
    }
}

std::vector<GltfNode> BuildGltfNodes(const tinygltf::Model& model)
{
    std::vector<GltfNode> nodes;
    nodes.reserve(model.nodes.size());
    for (const tinygltf::Node& src : model.nodes)
    {
        GltfNode node;
        node.mesh = src.mesh;
        node.skin = src.skin;
        node.children = src.children;
        node.local = NodeLocalMatrix(src);
        nodes.push_back(node);
    }

    // Root fix: drive the traversal from the default scene's root node list
    // instead of the old hasParent heuristic.
    const int sceneIndex = model.defaultScene >= 0
        ? model.defaultScene
        : (model.scenes.empty() ? -1 : 0);
    if (sceneIndex >= 0 && sceneIndex < static_cast<int>(model.scenes.size()))
    {
        for (int root : model.scenes[static_cast<size_t>(sceneIndex)].nodes)
        {
            ComputeNodeWorld(nodes, root, glm::mat4(1.0f));
        }
    }
    else
    {
        // No scene at all: treat every node as a root so meshes stay reachable.
        for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
        {
            ComputeNodeWorld(nodes, i, glm::mat4(1.0f));
        }
    }
    return nodes;
}

void ParseEmbeddedImages(const tinygltf::Model& model, Mesh& outMesh)
{
    for (const tinygltf::Image& image : model.images)
    {
        EmbeddedImage embedded;
        embedded.name = image.name;
        embedded.mimeType = image.mimeType;

        // Keep the original bufferView bytes and let ResourceManager decode
        // them later, preserving the existing texture upload path.
        if (image.bufferView >= 0 && static_cast<size_t>(image.bufferView) < model.bufferViews.size())
        {
            const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(image.bufferView)];
            if (view.buffer >= 0 && static_cast<size_t>(view.buffer) < model.buffers.size())
            {
                const tinygltf::Buffer& buffer = model.buffers[static_cast<size_t>(view.buffer)];
                const size_t offset = static_cast<size_t>(view.byteOffset);
                const size_t length = static_cast<size_t>(view.byteLength);
                if (length > 0 && offset + length <= buffer.data.size())
                {
                    embedded.bytes.assign(
                        buffer.data.begin() + static_cast<std::ptrdiff_t>(offset),
                        buffer.data.begin() + static_cast<std::ptrdiff_t>(offset + length));
                }
            }
        }

        outMesh.embeddedImages.push_back(std::move(embedded));
    }
}

void ParseImportedMaterials(const tinygltf::Model& model, Mesh& outMesh)
{
    for (const tinygltf::Material& material : model.materials)
    {
        ImportedMaterial imported;
        imported.name = material.name;

        const std::vector<double>& baseColorFactor = material.pbrMetallicRoughness.baseColorFactor;
        if (baseColorFactor.size() >= 4)
        {
            imported.baseColor = glm::vec4(
                static_cast<float>(baseColorFactor[0]),
                static_cast<float>(baseColorFactor[1]),
                static_cast<float>(baseColorFactor[2]),
                static_cast<float>(baseColorFactor[3]));
        }

        const int baseColorTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
        if (baseColorTextureIndex >= 0 && static_cast<size_t>(baseColorTextureIndex) < model.textures.size())
        {
            const int source = model.textures[static_cast<size_t>(baseColorTextureIndex)].source;
            if (source >= 0)
            {
                imported.baseColorImage = source;
            }
        }

        imported.alphaMode = material.alphaMode.empty() ? "OPAQUE" : material.alphaMode;
        imported.alphaBlend = material.alphaMode == "BLEND";
        imported.alphaMask = material.alphaMode == "MASK";
        imported.unlit = material.extensions.count("KHR_materials_unlit") > 0 ||
            material.extensions.count("VRMC_materials_mtoon") > 0;
        imported.alphaCutoff = static_cast<float>(material.alphaCutoff);
        if (imported.alphaMask)
        {
            imported.alphaCutoff = std::min(imported.alphaCutoff, 0.08f);
        }

        outMesh.importedMaterials.push_back(std::move(imported));
    }
}

bool AppendGltfPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const glm::mat4& nodeWorld,
    Mesh& outMesh)
{
    const auto positionIt = primitive.attributes.find("POSITION");
    if (positionIt == primitive.attributes.end() || positionIt->second < 0)
    {
        return false;
    }
    if (static_cast<size_t>(positionIt->second) >= model.accessors.size())
    {
        return false;
    }

    const tinygltf::Accessor& positionAccessor = model.accessors[static_cast<size_t>(positionIt->second)];
    if (positionAccessor.count <= 0 || positionAccessor.type != TINYGLTF_TYPE_VEC3)
    {
        return false;
    }

    size_t positionStride = 0;
    const unsigned char* positionData = AccessorData(model, positionAccessor, positionStride);
    if (positionData == nullptr)
    {
        return false;
    }

    const int materialIndex = primitive.material;
    glm::vec4 primitiveColor{1.0f, 1.0f, 1.0f, 1.0f};
    if (materialIndex >= 0 && static_cast<size_t>(materialIndex) < outMesh.importedMaterials.size())
    {
        primitiveColor = outMesh.importedMaterials[static_cast<size_t>(materialIndex)].baseColor;
    }

    const auto normalIt = primitive.attributes.find("NORMAL");
    const tinygltf::Accessor* normalAccessor =
        (normalIt != primitive.attributes.end() && normalIt->second >= 0 &&
         static_cast<size_t>(normalIt->second) < model.accessors.size())
            ? &model.accessors[static_cast<size_t>(normalIt->second)]
            : nullptr;
    size_t normalStride = 0;
    const unsigned char* normalData =
        (normalAccessor != nullptr && normalAccessor->type == TINYGLTF_TYPE_VEC3)
            ? AccessorData(model, *normalAccessor, normalStride)
            : nullptr;

    const auto uvIt = primitive.attributes.find("TEXCOORD_0");
    const tinygltf::Accessor* uvAccessor =
        (uvIt != primitive.attributes.end() && uvIt->second >= 0 &&
         static_cast<size_t>(uvIt->second) < model.accessors.size())
            ? &model.accessors[static_cast<size_t>(uvIt->second)]
            : nullptr;
    size_t uvStride = 0;
    const unsigned char* uvData =
        (uvAccessor != nullptr && uvAccessor->type == TINYGLTF_TYPE_VEC2)
            ? AccessorData(model, *uvAccessor, uvStride)
            : nullptr;

    const uint32_t baseVertex = static_cast<uint32_t>(outMesh.vertices.size());
    const uint32_t indexStart = static_cast<uint32_t>(outMesh.indices.size());
    outMesh.vertices.reserve(outMesh.vertices.size() + positionAccessor.count);

    // normalMatrix is the inverse-transpose of nodeWorld. CPU pre-bake path:
    // only the xyz of normals are transformed here, so the mat3 form is correct
    // (the GPU upload path in RendererApp keeps mat4 to match uniform mat4).
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(nodeWorld)));

    for (size_t i = 0; i < positionAccessor.count; ++i)
    {
        float positionValues[3] = {0.0f, 0.0f, 0.0f};
        ReadAttribute(
            positionData + i * positionStride,
            positionAccessor.componentType,
            positionAccessor.normalized,
            3,
            positionValues);

        Vertex vertex{};
        vertex.position = glm::vec3(nodeWorld * glm::vec4(
            positionValues[0], positionValues[1], positionValues[2], 1.0f));
        vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        vertex.uv = glm::vec2(0.0f, 0.0f);
        vertex.color = primitiveColor;

        if (normalData != nullptr && i < normalAccessor->count)
        {
            float normalValues[3] = {0.0f, 0.0f, 0.0f};
            ReadAttribute(
                normalData + i * normalStride,
                normalAccessor->componentType,
                normalAccessor->normalized,
                3,
                normalValues);
            vertex.normal = glm::normalize(normalMatrix * glm::vec3(
                normalValues[0], normalValues[1], normalValues[2]));
        }

        if (uvData != nullptr && i < uvAccessor->count)
        {
            float uvValues[2] = {0.0f, 0.0f};
            ReadAttribute(
                uvData + i * uvStride,
                uvAccessor->componentType,
                uvAccessor->normalized,
                2,
                uvValues);
            vertex.uv = glm::vec2(uvValues[0], uvValues[1]);
        }

        outMesh.vertices.push_back(vertex);
    }

    outMesh.hasNormals = outMesh.hasNormals || (normalData != nullptr);
    outMesh.hasTexcoords = outMesh.hasTexcoords || (uvData != nullptr);

    if (primitive.indices >= 0 && static_cast<size_t>(primitive.indices) < model.accessors.size())
    {
        const tinygltf::Accessor& indexAccessor = model.accessors[static_cast<size_t>(primitive.indices)];
        size_t indexStride = 0;
        const unsigned char* indexData = AccessorData(model, indexAccessor, indexStride);
        if (indexData != nullptr)
        {
            outMesh.indices.reserve(outMesh.indices.size() + indexAccessor.count);
            for (size_t i = 0; i < indexAccessor.count; ++i)
            {
                const uint32_t index = ReadIndex(indexData + i * indexStride, indexAccessor.componentType);
                outMesh.indices.push_back(baseVertex + index);
            }
        }
        else
        {
            // Fall back to a degenerate-free sequential index list.
            outMesh.indices.reserve(outMesh.indices.size() + positionAccessor.count);
            for (size_t i = 0; i < positionAccessor.count; ++i)
            {
                outMesh.indices.push_back(baseVertex + static_cast<uint32_t>(i));
            }
        }
    }
    else
    {
        outMesh.indices.reserve(outMesh.indices.size() + positionAccessor.count);
        for (size_t i = 0; i < positionAccessor.count; ++i)
        {
            outMesh.indices.push_back(baseVertex + static_cast<uint32_t>(i));
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

bool LoadGltfFromModel(const std::wstring& path, const tinygltf::Model& model, Mesh& outMesh)
{
    outMesh = {};
    outMesh.sourcePath = WideToUtf8(path);
    outMesh.displayName = std::filesystem::path(path).filename().string();

    ParseEmbeddedImages(model, outMesh);
    ParseImportedMaterials(model, outMesh);

    const std::vector<GltfNode> nodes = BuildGltfNodes(model);
    outMesh.gltfNodeCount = static_cast<int>(nodes.size());
    for (const GltfNode& node : nodes)
    {
        if (node.mesh >= 0)
        {
            ++outMesh.gltfMeshNodeCount;
        }
    }
    outMesh.gltfSkinCount = static_cast<int>(model.skins.size());
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
        if (node.mesh < 0 || static_cast<size_t>(node.mesh) >= model.meshes.size())
        {
            continue;
        }

        const tinygltf::Mesh& mesh = model.meshes[static_cast<size_t>(node.mesh)];
        for (const tinygltf::Primitive& primitive : mesh.primitives)
        {
            appendedAnyPrimitive = AppendGltfPrimitive(model, primitive, node.world, outMesh) || appendedAnyPrimitive;
        }
    }

    if (!appendedAnyPrimitive)
    {
        return false;
    }

    UpdateMeshBounds(outMesh);
    return !outMesh.vertices.empty() && !outMesh.indices.empty();
}

void GenerateNormals(Mesh& mesh)
{
    for (Vertex& vertex : mesh.vertices)
    {
        vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        Vertex& a = mesh.vertices[mesh.indices[i + 0]];
        Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        const glm::vec3 normal = glm::cross(b.position - a.position, c.position - a.position);
        a.normal += normal;
        b.normal += normal;
        c.normal += normal;
    }

    for (Vertex& vertex : mesh.vertices)
    {
        vertex.normal = normalizeOr(vertex.normal, glm::vec3(0.0f, 1.0f, 0.0f));
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

void AppendBox(Mesh& mesh, glm::vec3 center, glm::vec3 halfExtents)
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

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::unordered_map<ObjKey, uint32_t, ObjKeyHash> vertexMap;

    const glm::vec3 fallbackPosition{0.0f, 0.0f, 0.0f};
    const glm::vec3 fallbackNormal{0.0f, 1.0f, 0.0f};
    const glm::vec2 fallbackUv{0.0f, 0.0f};

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "v")
        {
            glm::vec3 value{};
            stream >> value.x >> value.y >> value.z;
            positions.push_back(value);
        }
        else if (type == "vn")
        {
            glm::vec3 value{};
            stream >> value.x >> value.y >> value.z;
            normals.push_back(value);
        }
        else if (type == "vt")
        {
            glm::vec2 value{};
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
    const std::string utf8Path = WideToUtf8(path);

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;

    const std::filesystem::path fsPath(path);
    const std::wstring extension = fsPath.extension().wstring();
    bool loaded = false;
    if (extension == L".glb" || extension == L".GLB")
    {
        loaded = loader.LoadBinaryFromFile(&model, &err, &warn, utf8Path);
    }
    else
    {
        // LoadASCIIFromFile resolves external .bin buffers relative to the
        // .gltf file's directory automatically.
        loaded = loader.LoadASCIIFromFile(&model, &err, &warn, utf8Path);
    }

    if (!warn.empty())
    {
        LogWarning("glTF import warning for '" + utf8Path + "':\n" + warn);
    }

    if (!loaded)
    {
        LogError("glTF import failed for '" + utf8Path + "':\n" + (err.empty() ? "unknown tinygltf error" : err));
        return false;
    }

    if (!LoadGltfFromModel(path, model, outMesh))
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
        mesh.boundsMin = glm::vec3(0.0f, 0.0f, 0.0f);
        mesh.boundsMax = glm::vec3(0.0f, 0.0f, 0.0f);
        return;
    }

    glm::vec3 minValue{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    glm::vec3 maxValue{
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
    const glm::vec3 center{
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
