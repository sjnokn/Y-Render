# Y-Render Architecture

Y-Render is organized as a small learning renderer rather than a full engine. The split mirrors the minimum Unity concepts needed before building shader effects.

## Source Layout

| Folder | Responsibility | Unity Analogy |
| --- | --- | --- |
| `src/Core` | Platform utilities, path lookup, HRESULT error handling | Low-level engine platform layer |
| `src/Scene` | Plain data types for mesh, transform, material, render targets | GameObject component data |
| `src/Assets` | OBJ loading and procedural mesh creation | Asset import pipeline, simplified |
| `src/Renderer` | Window, D3D11 device, render passes, shader binding, post-process, debug status | Render pipeline and Game view |
| `assets/shaders` | HLSL programs for scene and post effects | ShaderLab/URP shader library |

## Current Render Flow

1. `YRender.cpp` initializes COM and starts `RendererApp`.
2. `RendererApp` creates a Win32 window and Direct3D 11 device resources.
3. Assets are loaded or generated into CPU-side `Mesh` data.
4. Meshes are uploaded into vertex/index buffers.
5. The scene pass renders `SceneObject` instances into an offscreen render target.
6. The post-process pass samples the scene render target and writes to the swap-chain back buffer.

## Near-Term Improvements

| Improvement | Why It Matters |
| --- | --- |
| `RenderDevice` wrapper | Separates D3D object lifetime from app logic |
| `Scene` container | Stops demo code from owning all objects directly |
| `MaterialInstance` and texture slots | Prepares normal maps, BRDF demos, and shader variants |
| ImGui debug frontend | Replaces title-bar status with an inspector-style panel backed by `DebugState` |
| Depth texture SRV | Enables fog, depth of field, outlines, and screen-space effects |

ImGui is the preferred debug UI target, but it should be added as a deliberate dependency step rather than mixed into the first refactor.
