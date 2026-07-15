# Y-Render

Y-Render is a small OpenGL learning renderer focused on building a solid real-time rendering foundation before implementing the shader effects from *Shaders for Game Programming and Artists*.

## Current Baseline

- Win32 application loop and input handling
- OpenGL context, double-buffered presentation, framebuffer target, depth buffer, viewport
- Shader compilation from GLSL files
- Mesh, transform, camera, material, directional light
- GLB/glTF character as the default material test model, with a procedural shader dog fallback
- OBJ model loading with a generated fallback mesh
- Procedural texture plus WIC texture loading support
- Scene render target and fullscreen presentation pass
- Full-viewport Showcase Mode with auto-framed orbit camera and turntable
- Studio Lighting and Toon + Rim demo presets with before/after comparison
- Dissolve + Noise core effect with animated edge emission
- Demo switching and debug status in the window title
- TA-focused ImGui shader lab with object selection, Inspector tabs, debug views, and shader reload status
- Separate lighting-model and surface-effect controls in the Inspector
- First stylized GLSL lighting model: Toon Ramp + Rim
- Separated source layout for Core, Scene, Assets, and Renderer modules

## Build

Run from a Developer PowerShell or normal PowerShell:

```powershell
.\build.ps1
.\build.ps1 -Configuration Release
.\scripts\verify.ps1
```

The script locates Visual Studio through `vswhere`, builds `YRender.vcxproj`, and places the executable under `build\bin\`.

## Controls

- Mouse drag / Arrow keys: orbit around the character
- Mouse wheel or `W/S`: zoom
- `A/D` and `Q/E`: pan the orbit target
- `R` / `Home`: reset and frame the character
- `Tab` or `1/2/3`: switch demo
- `Space`: compare the preset effect with its baseline
- `T`: toggle the automatic turntable
- `F1`: toggle wireframe
- `F2`: switch between Showcase Mode and the ImGui shader lab
- `F5`: reload shaders
- `F9`: capture PNG screenshot
- `Esc`: quit

## Roadmap

The first target is the 60% foundation: a renderer that can load and draw textured meshes with camera control, basic lighting, render-to-texture, post-processing, debug feedback, and isolated demo scenes. After that, book effects such as blur, heat haze, HDR/bloom, reflections, normal mapping, toon shading, fog, shadows, and displacement can be added as focused demos.

See `docs/architecture.md` for the current module layout and `docs/ta-shader-lab-plan.md` for the technical-art shader roadmap.
