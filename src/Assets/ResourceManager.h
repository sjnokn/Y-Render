#pragma once

#include "Scene/SceneTypes.h"

#include <wincodec.h>

#include <cstddef>
#include <string>

namespace YRender
{
class ResourceManager
{
public:
    void Initialize();

    ShaderProgram CreateStandardShader();
    ShaderProgram CreatePostShader();
    ShaderProgram CreateDissolveParticleShader();
    ShaderProgram CreateBurnFragmentShader();
    void UploadMesh(Mesh& mesh);
    void UploadEmbeddedTextures(Mesh& mesh);
    Texture CreateCheckerTexture(unsigned int width, unsigned int height);
    Texture CreateTextureFromRgba(unsigned int width, unsigned int height, const void* pixels, unsigned int pitch);
    Texture LoadTextureWic(const std::wstring& path);
    Texture LoadTextureWicFromMemory(const void* data, size_t size);

private:
    ShaderProgram CreateShaderProgram(const std::wstring& vertexPath, const std::wstring& fragmentPath);
    unsigned int CompileShader(const std::wstring& path, unsigned int type);
};
} // namespace YRender
