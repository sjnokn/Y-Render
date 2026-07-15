#include "Renderer/OpenGL.h"

namespace YRender
{
PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
PFNGLATTACHSHADERPROC glAttachShader = nullptr;
PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
PFNGLBLENDEQUATIONPROC glBlendEquation = nullptr;
PFNGLBUFFERDATAPROC glBufferData = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
PFNGLCREATESHADERPROC glCreateShader = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
PFNGLDELETESHADERPROC glDeleteShader = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
PFNGLDRAWELEMENTSBASEVERTEXPROC glDrawElementsBaseVertex = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
PFNGLUNIFORM1FPROC glUniform1f = nullptr;
PFNGLUNIFORM1IPROC glUniform1i = nullptr;
PFNGLUNIFORM2FPROC glUniform2f = nullptr;
PFNGLUNIFORM3FPROC glUniform3f = nullptr;
PFNGLUNIFORM4FPROC glUniform4f = nullptr;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;

namespace
{
void* GetGlProc(const char* name)
{
    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (!proc || proc == reinterpret_cast<void*>(0x1) || proc == reinterpret_cast<void*>(0x2) ||
        proc == reinterpret_cast<void*>(0x3) || proc == reinterpret_cast<void*>(-1))
    {
        HMODULE module = GetModuleHandleW(L"opengl32.dll");
        proc = module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
    }
    return proc;
}

template <typename T>
bool Load(T& target, const char* name, bool required = true)
{
    target = reinterpret_cast<T>(GetGlProc(name));
    return target != nullptr || !required;
}
} // namespace

bool LoadOpenGLFunctions()
{
    bool ok = true;
    ok &= Load(glActiveTexture, "glActiveTexture");
    ok &= Load(glAttachShader, "glAttachShader");
    ok &= Load(glBindAttribLocation, "glBindAttribLocation");
    ok &= Load(glBindBuffer, "glBindBuffer");
    ok &= Load(glBindFramebuffer, "glBindFramebuffer");
    ok &= Load(glBindVertexArray, "glBindVertexArray");
    ok &= Load(glBlendEquation, "glBlendEquation");
    ok &= Load(glBufferData, "glBufferData");
    ok &= Load(glCheckFramebufferStatus, "glCheckFramebufferStatus");
    ok &= Load(glCompileShader, "glCompileShader");
    ok &= Load(glCreateProgram, "glCreateProgram");
    ok &= Load(glCreateShader, "glCreateShader");
    ok &= Load(glDeleteBuffers, "glDeleteBuffers");
    ok &= Load(glDeleteFramebuffers, "glDeleteFramebuffers");
    ok &= Load(glDeleteProgram, "glDeleteProgram");
    ok &= Load(glDeleteShader, "glDeleteShader");
    ok &= Load(glDeleteVertexArrays, "glDeleteVertexArrays");
    ok &= Load(glDisableVertexAttribArray, "glDisableVertexAttribArray");
    Load(glDrawElementsBaseVertex, "glDrawElementsBaseVertex", false);
    ok &= Load(glEnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= Load(glFramebufferTexture2D, "glFramebufferTexture2D");
    ok &= Load(glGenBuffers, "glGenBuffers");
    ok &= Load(glGenFramebuffers, "glGenFramebuffers");
    ok &= Load(glGenVertexArrays, "glGenVertexArrays");
    ok &= Load(glGetAttribLocation, "glGetAttribLocation");
    ok &= Load(glGetProgramInfoLog, "glGetProgramInfoLog");
    ok &= Load(glGetProgramiv, "glGetProgramiv");
    ok &= Load(glGetShaderInfoLog, "glGetShaderInfoLog");
    ok &= Load(glGetShaderiv, "glGetShaderiv");
    ok &= Load(glGetUniformLocation, "glGetUniformLocation");
    ok &= Load(glLinkProgram, "glLinkProgram");
    ok &= Load(glShaderSource, "glShaderSource");
    ok &= Load(glUniform1f, "glUniform1f");
    ok &= Load(glUniform1i, "glUniform1i");
    ok &= Load(glUniform2f, "glUniform2f");
    ok &= Load(glUniform3f, "glUniform3f");
    ok &= Load(glUniform4f, "glUniform4f");
    ok &= Load(glUniformMatrix4fv, "glUniformMatrix4fv");
    ok &= Load(glUseProgram, "glUseProgram");
    ok &= Load(glVertexAttribPointer, "glVertexAttribPointer");
    Load(wglSwapIntervalEXT, "wglSwapIntervalEXT", false);
    return ok;
}
} // namespace YRender
