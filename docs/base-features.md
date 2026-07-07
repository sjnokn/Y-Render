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

## Next Foundation Improvements

| Area | Feature |
| --- | --- |
| Debug UI | Add ImGui once dependency setup is ready |
| Assets | Add glTF loading and texture path resolution |
| Materials | Normal/specular maps and serialized material files |
| Rendering | Multiple lights, transparent queue, skybox |
| Tooling | Screenshot capture, RenderDoc-friendly frame markers |
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
