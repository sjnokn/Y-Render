# Y-Render Architecture

Y-Render is organized as a small learning renderer rather than a full engine. The split mirrors the minimum Unity concepts needed before building shader effects.

## Source Layout

| Folder | Responsibility | Unity Analogy |
| --- | --- | --- |
| `src/Core` | Platform utilities, path lookup, HRESULT error handling | Low-level engine platform layer |
| `src/Scene` | Plain data types for mesh, transform, material, render targets | GameObject component data |
| `src/Assets` | OBJ loading and procedural mesh creation | Asset import pipeline, simplified |
| `src/Assets` | Resource upload, shader compilation, texture creation | Asset database/resource manager, simplified |
| `src/Renderer` | Window, D3D11 device wrapper, render passes, shader binding, post-process, ImGui debug UI | Render pipeline and Game view |
| `assets/shaders` | HLSL programs for scene and post effects | ShaderLab/URP shader library |
| `external/imgui` | Dear ImGui debug UI dependency | Inspector/Stats window, simplified |

## Current Render Flow

1. `YRender.cpp` initializes COM and starts `RendererApp`.
2. `RendererApp` creates a Win32 window and asks `RenderDevice` to own Direct3D 11 device resources.
3. `ResourceManager` creates shaders, constant buffers, textures, and GPU mesh buffers.
4. `Scene` owns the current demo's `SceneObject` list.
5. The scene pass renders `SceneObject` instances into an offscreen render target.
6. The post-process pass samples the scene render target and writes to the swap-chain back buffer.
7. ImGui draws the debug panel on top of the back buffer.

## Near-Term Improvements

| Improvement | Why It Matters |
| --- | --- |
| `MaterialInstance` and texture slots | Prepares normal maps, BRDF demos, and shader variants |
| Depth texture SRV | Enables fog, depth of field, outlines, and screen-space effects |

ImGui is the preferred debug UI target, but it should be added as a deliberate dependency step rather than mixed into the first refactor.
