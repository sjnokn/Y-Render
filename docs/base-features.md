# Renderer Baseline Feature Checklist

## 60% Foundation

| Area | Feature | Status |
| --- | --- | --- |
| Application | Win32 window, message loop, frame timer | Done |
| Graphics API | D3D11 device, swap chain, back buffer, viewport | Done |
| Depth | Depth buffer and depth state | Done |
| Resources | Vertex/index buffers, constant buffers, shaders, textures, samplers | Done |
| Mesh | Built-in mesh generation and OBJ loading | Done |
| Camera | Perspective camera with free movement | Done |
| Transform | Position, rotation, scale, model matrix | Done |
| Shader | HLSL compile and bind for vertex/pixel shaders | Done |
| Material | Base color, texture flag, specular power | Done |
| Texture | Procedural checker texture and WIC image loader | Done |
| Lighting | Ambient, directional diffuse, Blinn-Phong specular | Done |
| Render Target | Offscreen scene color target | Done |
| Post Process | Fullscreen pass with multiple modes | Done |
| Debug | FPS/status title, wireframe toggle, shader compile errors | Done |
| Demos | Switchable primitive/OBJ scenes | Done |
| Structure | Core/Scene/Assets/Renderer source modules | Done |
| Debug UI | ImGui inspector-style debug panel | Done |
| Render Device | D3D11 device, swap-chain, targets, states isolated in `RenderDevice` | Done |
| Scene Container | Scene object ownership isolated in `Scene` | Done |
| Resource Manager | Shader, buffer, mesh, and texture creation isolated in `ResourceManager` | Done |
| Camera Class | Camera movement and projection isolated in `Camera` | Done |
| Input System | Key state isolated in `InputState` | Done |
| Material Slots | Albedo, normal, and specular slots represented on `Material` | Done |
| Depth Texture SRV | Depth buffer can be sampled by shaders | Done |
| Shader Includes | Shared HLSL lighting include added | Done |
| Shader Reload | Runtime shader reload via F5 and debug UI | Done |
| Multiple Lights | Basic directional light list and shader loop | Done |
| Project Config | `config/y-render.ini` controls startup basics | Done |
| glTF Entry | Minimal external-buffer glTF mesh loader path | Done |
| Screenshot | PNG screenshot capture via F9 | Done |
| Frame Stats | Draw calls, triangles, passes, frame time | Done |
| Logging | OutputDebugString and `logs/y-render.log` | Done |
| GPU Markers | D3D11 user-defined annotation events | Done |
| Verification | Debug/Release verify script | Done |

## Next Foundation Improvements

| Area | Feature |
| --- | --- |
| Assets | Broaden glTF coverage to skinning, materials, and embedded buffers |
| Materials | Serialized material files and real normal/specular texture sampling |
| Rendering | Transparent queue and skybox |
| Tooling | In-app texture/depth viewers and screenshot naming UI |
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
