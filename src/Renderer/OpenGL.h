#pragma once

#include <windows.h>
#include <gl/GL.h>

#include <cstddef>

#ifndef APIENTRYP
#define APIENTRYP APIENTRY*
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_DEPTH_COMPONENT
#define GL_DEPTH_COMPONENT 0x1902
#endif
#ifndef GL_UNSIGNED_INT_24_8
#define GL_UNSIGNED_INT_24_8 0x84FA
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif

namespace YRender
{
using GLchar = char;
using GLsizeiptr = ptrdiff_t;
using GLintptr = ptrdiff_t;

using PFNGLACTIVETEXTUREPROC = void(APIENTRYP)(GLenum texture);
using PFNGLATTACHSHADERPROC = void(APIENTRYP)(GLuint program, GLuint shader);
using PFNGLBINDBUFFERPROC = void(APIENTRYP)(GLenum target, GLuint buffer);
using PFNGLBINDATTRIBLOCATIONPROC = void(APIENTRYP)(GLuint program, GLuint index, const GLchar* name);
using PFNGLBINDFRAMEBUFFERPROC = void(APIENTRYP)(GLenum target, GLuint framebuffer);
using PFNGLBINDVERTEXARRAYPROC = void(APIENTRYP)(GLuint array);
using PFNGLBLENDEQUATIONPROC = void(APIENTRYP)(GLenum mode);
using PFNGLBUFFERDATAPROC = void(APIENTRYP)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
using PFNGLCHECKFRAMEBUFFERSTATUSPROC = GLenum(APIENTRYP)(GLenum target);
using PFNGLCOMPILESHADERPROC = void(APIENTRYP)(GLuint shader);
using PFNGLCREATEPROGRAMPROC = GLuint(APIENTRYP)();
using PFNGLCREATESHADERPROC = GLuint(APIENTRYP)(GLenum type);
using PFNGLDELETEBUFFERSPROC = void(APIENTRYP)(GLsizei n, const GLuint* buffers);
using PFNGLDELETEFRAMEBUFFERSPROC = void(APIENTRYP)(GLsizei n, const GLuint* framebuffers);
using PFNGLDELETEPROGRAMPROC = void(APIENTRYP)(GLuint program);
using PFNGLDELETESHADERPROC = void(APIENTRYP)(GLuint shader);
using PFNGLDELETEVERTEXARRAYSPROC = void(APIENTRYP)(GLsizei n, const GLuint* arrays);
using PFNGLDISABLEVERTEXATTRIBARRAYPROC = void(APIENTRYP)(GLuint index);
using PFNGLDRAWELEMENTSBASEVERTEXPROC = void(APIENTRYP)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void(APIENTRYP)(GLuint index);
using PFNGLFRAMEBUFFERTEXTURE2DPROC = void(APIENTRYP)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
using PFNGLGENBUFFERSPROC = void(APIENTRYP)(GLsizei n, GLuint* buffers);
using PFNGLGENFRAMEBUFFERSPROC = void(APIENTRYP)(GLsizei n, GLuint* framebuffers);
using PFNGLGENVERTEXARRAYSPROC = void(APIENTRYP)(GLsizei n, GLuint* arrays);
using PFNGLGETATTRIBLOCATIONPROC = GLint(APIENTRYP)(GLuint program, const GLchar* name);
using PFNGLGETPROGRAMINFOLOGPROC = void(APIENTRYP)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLGETPROGRAMIVPROC = void(APIENTRYP)(GLuint program, GLenum pname, GLint* params);
using PFNGLGETSHADERINFOLOGPROC = void(APIENTRYP)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLGETSHADERIVPROC = void(APIENTRYP)(GLuint shader, GLenum pname, GLint* params);
using PFNGLGETUNIFORMLOCATIONPROC = GLint(APIENTRYP)(GLuint program, const GLchar* name);
using PFNGLLINKPROGRAMPROC = void(APIENTRYP)(GLuint program);
using PFNGLSHADERSOURCEPROC = void(APIENTRYP)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
using PFNGLUNIFORM1FPROC = void(APIENTRYP)(GLint location, GLfloat v0);
using PFNGLUNIFORM1IPROC = void(APIENTRYP)(GLint location, GLint v0);
using PFNGLUNIFORM2FPROC = void(APIENTRYP)(GLint location, GLfloat v0, GLfloat v1);
using PFNGLUNIFORM3FPROC = void(APIENTRYP)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
using PFNGLUNIFORM4FPROC = void(APIENTRYP)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
using PFNGLUNIFORMMATRIX4FVPROC = void(APIENTRYP)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUSEPROGRAMPROC = void(APIENTRYP)(GLuint program);
using PFNGLVERTEXATTRIBPOINTERPROC = void(APIENTRYP)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
using PFNWGLSWAPINTERVALEXTPROC = BOOL(WINAPI*)(int interval);

extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
extern PFNGLBLENDEQUATIONPROC glBlendEquation;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLDRAWELEMENTSBASEVERTEXPROC glDrawElementsBaseVertex;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLUNIFORM3FPROC glUniform3f;
extern PFNGLUNIFORM4FPROC glUniform4f;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
extern PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

bool LoadOpenGLFunctions();
} // namespace YRender
