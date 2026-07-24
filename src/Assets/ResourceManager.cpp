#include "Assets/ResourceManager.h"

#include "Core/Platform.h"
#include "Renderer/OpenGL.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <stb_image.h>

namespace YRender
{
void ResourceManager::Initialize()
{
    // stb_image returns images top-down (first row = top), matching the GL texture
    // origin used by glTexImage2D below. Pin it so a stray flip toggle elsewhere
    // cannot silently invert textures.
    stbi_set_flip_vertically_on_load(false);
}

ShaderProgram ResourceManager::CreateStandardShader()
{
    return CreateShaderProgram(FindAsset(L"assets\\shaders\\Standard.vert"), FindAsset(L"assets\\shaders\\Standard.frag"));
}

ShaderProgram ResourceManager::CreatePostShader()
{
    return CreateShaderProgram(FindAsset(L"assets\\shaders\\PostProcess.vert"), FindAsset(L"assets\\shaders\\PostProcess.frag"));
}

ShaderProgram ResourceManager::CreateDissolveParticleShader()
{
    return CreateShaderProgram(
        FindAsset(L"assets\\shaders\\DissolveParticle.vert"),
        FindAsset(L"assets\\shaders\\DissolveParticle.frag"));
}

ShaderProgram ResourceManager::CreateBurnFragmentShader()
{
    return CreateShaderProgram(
        FindAsset(L"assets\\shaders\\BurnFragment.vert"),
        FindAsset(L"assets\\shaders\\BurnFragment.frag"));
}

void ResourceManager::UploadMesh(Mesh& mesh)
{
    if (mesh.vao != 0)
    {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vertexBuffer);
        glDeleteBuffers(1, &mesh.indexBuffer);
    }

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vertexBuffer);
    glGenBuffers(1, &mesh.indexBuffer);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)), mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)), mesh.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, color)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ResourceManager::UploadEmbeddedTextures(Mesh& mesh)
{
    for (EmbeddedImage& image : mesh.embeddedImages)
    {
        if (image.texture != 0 || image.bytes.empty())
        {
            continue;
        }

        try
        {
            Texture texture = LoadTextureWicFromMemory(image.bytes.data(), image.bytes.size());
            image.texture = texture.id;
            image.width = texture.width;
            image.height = texture.height;
        }
        catch (const std::exception&)
        {
            image.texture = 0;
            image.width = 0;
            image.height = 0;
        }
    }
}

Texture ResourceManager::CreateCheckerTexture(unsigned int width, unsigned int height)
{
    std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            const bool bright = ((x / 16) + (y / 16)) % 2 == 0;
            const uint8_t r = bright ? 235 : 68;
            const uint8_t g = bright ? 235 : 86;
            const uint8_t b = bright ? 225 : 112;
            pixels[static_cast<size_t>(y) * width + x] = 0xff000000u | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | r;
        }
    }

    return CreateTextureFromRgba(width, height, pixels.data(), width * sizeof(uint32_t));
}

Texture ResourceManager::CreateTextureFromRgba(unsigned int width, unsigned int height, const void* pixels, unsigned int pitch)
{
    Texture result;
    result.width = width;
    result.height = height;
    glGenTextures(1, &result.id);
    glBindTexture(GL_TEXTURE_2D, result.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (pitch == width * 4)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    else
    {
        std::vector<uint8_t> tightlyPacked(static_cast<size_t>(width) * height * 4);
        const auto* source = static_cast<const uint8_t*>(pixels);
        for (unsigned int y = 0; y < height; ++y)
        {
            std::copy(source + static_cast<size_t>(y) * pitch, source + static_cast<size_t>(y) * pitch + width * 4, tightlyPacked.data() + static_cast<size_t>(y) * width * 4);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, tightlyPacked.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return result;
}

Texture ResourceManager::LoadTextureWic(const std::wstring& path)
{
    const std::string utf8Path = WideToUtf8(path);
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(utf8Path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        throw std::runtime_error("Failed to load texture '" + utf8Path + "': " + stbi_failure_reason());
    }

    Texture texture = CreateTextureFromRgba(static_cast<unsigned int>(width), static_cast<unsigned int>(height), pixels, static_cast<unsigned int>(width) * 4);
    stbi_image_free(pixels);
    return texture;
}

Texture ResourceManager::LoadTextureWicFromMemory(const void* data, size_t size)
{
    if (!data || size == 0)
    {
        throw std::runtime_error("Empty embedded texture");
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(data), static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        throw std::runtime_error(std::string("Failed to decode embedded texture: ") + stbi_failure_reason());
    }

    Texture texture = CreateTextureFromRgba(static_cast<unsigned int>(width), static_cast<unsigned int>(height), pixels, static_cast<unsigned int>(width) * 4);
    stbi_image_free(pixels);
    return texture;
}

ShaderProgram ResourceManager::CreateShaderProgram(const std::wstring& vertexPath, const std::wstring& fragmentPath)
{
    const GLuint vertexShader = CompileShader(vertexPath, GL_VERTEX_SHADER);
    const GLuint fragmentShader = CompileShader(fragmentPath, GL_FRAGMENT_SHADER);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aNormal");
    glBindAttribLocation(program, 2, "aUV");
    glBindAttribLocation(program, 3, "aColor");
    glBindAttribLocation(program, 4, "aBarycentric");
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<size_t>(std::max(logLength, 1)), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        throw std::runtime_error("GLSL link failed:\n" + log);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    ShaderProgram result;
    result.program = program;
    return result;
}

unsigned int ResourceManager::CompileShader(const std::wstring& path, unsigned int type)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Unable to open shader file: " + WideToUtf8(path));
    }

    std::stringstream stream;
    stream << file.rdbuf();
    const std::string source = stream.str();
    const char* sourceText = source.c_str();
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &sourceText, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<size_t>(std::max(logLength, 1)), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("GLSL compile failed for " + WideToUtf8(path) + ":\n" + log);
    }

    return shader;
}
} // namespace YRender
