# Renderer Baseline Feature Checklist

## 60% Foundation

| Area | Feature | Status |
| --- | --- | --- |
| Application | Win32 window, message loop, frame timer | Done |
| Window | Framebuffer Resize callback, viewport update, and projection refresh | Implemented; code-audited, targeted runtime resize evidence pending |
| Graphics API | OpenGL context, double-buffered back buffer, framebuffer target, viewport | Done; runtime-rendered |
| Rasterization | Triangle submission, vertex/index buffers, GLSL draw path, and raster state | Implemented; runtime-rendered |
| Depth | Depth buffer and depth state | Done; runtime A/B occlusion test verified |
| Resources | Vertex/index buffers, GLSL programs, textures, samplers | Done |
| Mesh | Built-in mesh generation and OBJ loading | Done |
| Camera | Perspective camera with free movement | Done; code and runtime showcase/orbit path verified |
| Transform | Position, rotation, scale, model matrix, MVP path | Done; code-audited and runtime-rendered |
| Shader | GLSL compile and bind for vertex/fragment shaders | Done |
| Material | Base color, texture flag, specular power | Done |
| Texture | Procedural checker texture and WIC image loader | Done |
| Lighting | Ambient, directional diffuse, Blinn-Phong specular | Done |
| Render Target | Offscreen scene color target | Done |
| Presentation Pass | Fullscreen scene-color pass with background treatment and Bloom composite | Done |
| Debug | FPS/status title, wireframe toggle, shader compile errors | Done |
| Demos | Switchable primitive/OBJ scenes | Done |
| Structure | Core/Scene/Assets/Renderer source modules | Done |
| Shader Lab UI | ImGui hierarchy, Inspector tabs, shader status, and debug views | Done |
| Render Device | OpenGL context, targets, viewport and raster state isolated in `RenderDevice` | Done |
| Scene Container | Scene object ownership isolated in `Scene` | Done |
| Resource Manager | Shader, buffer, mesh, and texture creation isolated in `ResourceManager` | Done |
| Camera Class | Camera movement and projection isolated in `Camera` | Done |
| Input System | Key state isolated in `InputState` | Done |
| Material Slots | Albedo texture slot represented on `Material`; normal/specular slots deferred until real sampling is implemented | Partial |
| Depth Texture SRV | Depth buffer can be sampled by shaders | Done |
| Shader Includes | Shared lighting include retained as legacy reference; active shaders use GLSL | Deferred |
| Shader Reload | Runtime shader reload via F5 and debug UI | Done |
| Multiple Lights | Basic directional light list and shader loop | Done |
| Project Config | `config/y-render.ini` controls startup basics | Done |
| glTF Entry | GLB/glTF mesh loading, embedded image upload, and default `character.glb` path | Done; default GLB runtime load verified |
| Screenshot | PNG screenshot capture via F9 | Done |
| Frame Stats | Draw calls, triangles, passes, frame time | Done |
| Logging | OutputDebugString and `logs/y-render.log` | Done |
| GPU Markers | Render pass event hooks; native GPU markers are deferred for OpenGL | Partial |
| Toon/Rim Demo | First stylized lighting model with live material controls | Done |
| Dissolve Effects Demo | Eight switchable masks/styles: basic noise, directional, local sphere, incineration/ash, FlowMap energy, pixelated, edge ash, and smoke | Done; Bloom and dynamic particles are integrated |
| Blur/Bloom Demo | Bright-pass extraction, two-pass blur, composite, and live threshold/strength controls | Done |
| Depth Fog/Outline Demo | Distance fog and depth-based silhouette outline with live controls and a frozen preset | Done |
| Showcase Mode | Full-viewport startup, compact HUD, orbit framing, turntable, and baseline comparison | Done |
| Lighting Correctness | Inverse-transpose normal transform and albedo-independent Blinn-Phong specular | Done |
| Verification | Debug/Release verify script | Done |

## Next Foundation Improvements

| Area | Feature |
| --- | --- |
| Assets | Broaden glTF coverage to skinning/animation and richer material semantics |
| Materials | Serialized material files, real normal/specular texture sampling, and later glass/metal/PBR presets |
| Rendering | Transparent queue and skybox |
| Tooling | In-app texture/depth viewers and screenshot naming UI |
| Shader Lab | Material presets, shader error history, UV/normal/depth debug modes |
| Effects | Dissolve/noise, blur/bloom, depth fog/outline, heat haze/refraction |
| Validation | Golden image tests for stable shader demos |

## Book Effect Mapping

| Book Topic | Renderer Dependency |
| --- | --- |
| Color filters and blur | Render target + fullscreen post-process |
| Depth of field | Depth texture sampling + multi-pass blur |
| Heat haze/refraction | Scene color texture + distortion map |
| HDR/bloom | Floating-point render target + bright pass + blur |
| Pixel lighting/normal maps | Tangent space mesh data + material textures |
| Reflection/refraction | Cubemap textures + environment pass |
| BRDF/noise materials | Material parameter system + shared shader library |
| Toon/hatching | Lighting shader variants + screen/depth outline |
| Fog/atmosphere | Depth/world-position reconstruction |
| Animation/skinning | Mesh animation data + bone constant buffers |
| Shadows | Shadow map pass + light-space matrices |
| LOD/displacement | Mesh selection and vertex displacement shaders |

## Audit and Runtime Verification (2026-10-18)

The following status separates implementation evidence from targeted runtime evidence. “Implemented” means the code path exists; “Verified” means it was exercised in the built renderer or in a focused runtime experiment.

| Capability | Implementation status | Verification status and evidence |
| --- | --- | --- |
| MVP transforms | Implemented | Verified through the model/view/projection path audit and the rendered character frame; no standalone numeric matrix test is recorded. |
| Camera and perspective projection | Implemented | Camera movement, orbit framing, and perspective projection are present and visible in the runtime showcase; a separate projection-accuracy test is not recorded. |
| Rasterization and triangle drawing | Implemented | Verified by successful OpenGL draw submission and actual rendered geometry; no isolated triangle golden-image test is recorded. |
| Depth testing and occlusion | Implemented | Fully runtime-verified with the cross-occluding-object A/B experiment: depth enabled preserves the nearer surface, while depth disabled follows draw order. Evidence: `screenshots/y-render-20261018-200605.png` and `screenshots/y-render-20261018-200621.png`. |
| Window Resize | Implemented | Framebuffer-size handling, viewport update, and projection refresh are code-audited; an independent runtime resize capture remains pending. |
| Default `character.glb` loading | Implemented and repaired | Runtime-verified after removing the incompatible `TINYGLTF_NO_STB_IMAGE` definition: the built renderer loaded 129,374 vertices, 46,891 triangles, 17 submeshes, and 27 embedded images. Evidence: `screenshots/y-render-20261018-202006.png` and `logs/y-render.log`. Skinning remains a static preview. |
