# Y-Render Architecture

Y-Render is organized as a technical-art shader lab rather than a full engine. The split keeps enough editor-like surface area to select objects, edit material parameters, preview buffers, and build book-inspired shader effects.

See `docs/ta-shader-lab-plan.md` for the current shader-lab roadmap.

## Source Layout

| Folder | Responsibility | Unity Analogy |
| --- | --- | --- |
| `src/Core` | Platform utilities, path lookup, HRESULT error handling | Low-level engine platform layer |
| `src/Core` | Logging, input, project configuration | Player settings and diagnostics, simplified |
| `src/Scene` | Camera, scene object, light, mesh/material data | GameObject component data |
| `src/Assets` | OBJ/glTF loading and procedural mesh creation | Asset import pipeline, simplified |
| `src/Assets` | Resource upload, shader compilation, texture creation | Asset database/resource manager, simplified |
| `src/Renderer` | Window, OpenGL device wrapper, render passes, shader binding, presentation pass, ImGui shader lab | Render pipeline and Game view |
| `assets/shaders` | GLSL programs for scene and post effects | ShaderLab/URP shader library |
| `external/imgui` | Dear ImGui shader lab UI dependency | Inspector/Stats window, simplified |

## Current Render Flow

1. `YRender.cpp` initializes COM and starts `RendererApp`.
2. `RendererApp` creates a Win32 window and asks `RenderDevice` to own the OpenGL context, framebuffer targets, and viewport state.
3. `ResourceManager` compiles GLSL programs, uploads textures, and creates GPU mesh buffers. F5 rebuilds shaders transactionally at runtime.
4. `Scene` owns the current demo's `SceneObject` list.
5. The scene pass renders `SceneObject` instances into an offscreen render target.
6. The presentation pass samples the scene render target and writes to the double-buffered OpenGL back buffer.
7. ImGui draws either the compact Showcase HUD or the F2 shader-lab panels on top of the back buffer; F9 captures a PNG screenshot.

The default presentation path uses the entire client area. Enabling the shader lab with F2 changes the scene viewport to the center panel without changing the scene or presentation pass architecture.

## Near-Term Improvements

| Improvement | Why It Matters |
| --- | --- |
| Material presets | Preserves Toon, Dissolve, Hologram, and BRDF parameter sets |
| Expanded debug views | Makes UV, normal, depth, and render-target problems visible |
| Multi-pass render-target chain | Enables blur, bloom, heat haze, refraction, and depth effects |
