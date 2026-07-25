#pragma once

// Phase 4: unified matrix-upload convention.
//
// Every mat4 uniform upload (uWorld / uViewProj / uNormalMatrix) MUST go
// through this helper. GLM stores matrices column-major; we upload with
// GL_FALSE (no transpose) and glm::value_ptr. Do NOT reintroduce a manual
// transpose (the old Burn/Dissolve paths double-transposed and shipped
// wrong matrices), and do NOT switch to GL_TRUE.
//
// Uniform locations are re-queried every frame on purpose: F5 shader hot
// reload can change them, and caching would break after a reload.

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace YRender
{
inline void UploadMat4(GLuint program, const char* name, const glm::mat4& matrix)
{
    glUniformMatrix4fv(
        glGetUniformLocation(program, name),
        1,
        GL_FALSE,
        glm::value_ptr(matrix));
}
} // namespace YRender
