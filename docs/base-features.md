# Renderer Baseline Feature Checklist

## 60% Foundation

| Area | Feature | Status |
| --- | --- | --- |
| Application | Win32 window, message loop, frame timer | Done |
| Graphics API | OpenGL context, double-buffered back buffer, framebuffer target, viewport | Done |
| Depth | Depth buffer and depth state | Done |
| Resources | Vertex/index buffers, GLSL programs, textures, samplers | Done |
| Mesh | Built-in mesh generation and OBJ loading | Done |
| Camera | Perspective camera with free movement | Done |
| Transform | Position, rotation, scale, model matrix | Done |
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
| glTF Entry | Minimal external-buffer glTF mesh loader path | Done |
| Screenshot | PNG screenshot capture via F9 | Done |
| Frame Stats | Draw calls, triangles, passes, frame time | Done |
| Logging | OutputDebugString and `logs/y-render.log` | Done |
| GPU Markers | Render pass event hooks; native GPU markers are deferred for OpenGL | Partial |
| Toon/Rim Demo | First stylized lighting model with live material controls | Done |
| Dissolve/Noise Demo | World-space noise clipping, layered edge treatment, animated controls, and a reusable preset | Done; Bloom is now integrated |
| Blur/Bloom Demo | Bright-pass extraction, two-pass blur, composite, and live threshold/strength controls | Done |
| Showcase Mode | Full-viewport startup, compact HUD, orbit framing, turntable, and baseline comparison | Done |
| Lighting Correctness | Inverse-transpose normal transform and albedo-independent Blinn-Phong specular | Done |
| Verification | Debug/Release verify script | Done |

## Next Foundation Improvements

| Area | Feature |
| --- | --- |
| Assets | Broaden glTF coverage to skinning, materials, and embedded buffers |
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
