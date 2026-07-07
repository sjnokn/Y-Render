#pragma once

namespace YRender
{
struct FrameStats
{
    float deltaSeconds = 0.0f;
    float fps = 0.0f;
    int drawCalls = 0;
    int triangles = 0;
    int passes = 0;

    void Reset(float dt)
    {
        deltaSeconds = dt;
        drawCalls = 0;
        triangles = 0;
        passes = 0;
    }
};
} // namespace YRender
