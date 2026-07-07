#pragma once

#include "Scene/SceneTypes.h"

#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <string>

namespace YRender
{
class ResourceManager
{
public:
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    ShaderProgram CreateStandardShader();
    ShaderProgram CreatePostShader();
    void UploadMesh(Mesh& mesh);
    Texture CreateCheckerTexture(UINT width, UINT height);
    Texture CreateTextureFromRgba(UINT width, UINT height, const void* pixels, UINT pitch);
    Texture LoadTextureWic(const std::wstring& path);
    ComPtr<ID3D11Buffer> CreatePostQuad();
    ComPtr<ID3D11Buffer> CreateConstantBuffer(UINT byteWidth);

private:
    Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const char* entry, const char* target);

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
};
} // namespace YRender
