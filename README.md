# Y-Render

Y-Render is a small Direct3D 11 learning renderer focused on building a solid real-time rendering foundation before implementing the shader effects from *Shaders for Game Programming and Artists*.

## Current Baseline

- Win32 application loop and input handling
- Direct3D 11 device, swap chain, back buffer, depth buffer, viewport
- Shader compilation from HLSL files
- Mesh, transform, camera, material, directional light
- OBJ model loading with a generated fallback mesh
- Procedural texture plus WIC texture loading support
- Scene render target, fullscreen post-process pass, and simple effect switching
- Demo scene switching and debug status in the window title
- Separated source layout for Core, Scene, Assets, and Renderer modules

## Build

Run from a Developer PowerShell or normal PowerShell:

```powershell
.\build.ps1
```

The script locates Visual Studio through `vswhere`, builds `YRender.vcxproj`, and places the executable under `build\bin\`.

## Controls

- `W/A/S/D`: move camera
- `Q/E`: move camera down/up
- Arrow keys: rotate camera
- `Tab`: switch demo scene
- `1`: no post-process
- `2`: grayscale
- `3`: invert
- `4`: vignette
- `F1`: toggle wireframe
- `Esc`: quit

## Roadmap

The first target is the 60% foundation: a renderer that can load and draw textured meshes with camera control, basic lighting, render-to-texture, post-processing, debug feedback, and isolated demo scenes. After that, book effects such as blur, heat haze, HDR/bloom, reflections, normal mapping, toon shading, fog, shadows, and displacement can be added as focused demos.

See `docs/architecture.md` for the current module layout and the Unity-style concepts it maps to.
