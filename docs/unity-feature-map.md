# Y-Render and Unity Feature Map

Note: this file is retained as a historical comparison only. The active direction is now the technical-art shader lab roadmap in `docs/ta-shader-lab-plan.md`.

This checklist maps the current Y-Render foundation to the closest Unity editor or runtime concept. It is a practical orientation map, not a promise that the systems are equivalent in scale.

## Current Feature Comparison

| Area | Y-Render Current Feature | Unity Corresponding Position | Current Match | Gap / Next Step |
| --- | --- | --- | --- | --- |
| Application Shell | Win32 app, main loop, frame timer | Unity Player loop / Editor host window | Basic runtime loop exists | No editor docking/layout persistence yet |
| Window / Viewport | Direct3D swap chain and main render window | Game view | Scene is visible and interactive | No separate Scene view camera yet |
| Render Device | `RenderDevice` owns D3D11 device, swap chain, targets, states | Render pipeline backend | Core rendering backend exists | No pipeline asset or render graph abstraction |
| Scene Container | `Scene` owns `SceneObject` list and directional lights | Scene asset / active Scene | Runtime scene list exists | Scene is not serialized to disk yet |
| Scene Object | `SceneObject` has name, mesh, transform, material | GameObject with MeshRenderer-like data | Basic object data exists | No component system, hierarchy parenting, prefabs |
| Transform | Position, rotation, scale, world matrix | Transform component in Inspector | Editable in Inspector | No transform gizmo or parent-relative transform |
| Objects | ImGui `Objects` window lists objects and supports selection | Hierarchy panel | Basic selection exists | No create, duplicate, delete, drag reorder |
| Inspector | ImGui `Inspector` edits name, transform, material values | Inspector panel | Core object editing exists | No multi-select, undo, component add/remove |
| Toolbar | Reload shaders and scene controls | Unity toolbar / shader preview controls | Basic controls visible | View diagnostics live in Inspector |
| Asset Browser | ImGui `Assets` window lists built-in meshes, shaders, texture | Project panel | Static asset visibility exists | No filesystem scan, import metadata, drag/drop |
| Meshes | Procedural shader dog, plane, and OBJ loading | Mesh assets / Model importer | Mesh loading path exists | glTF/material coverage is still minimal |
| Materials | Base color, albedo texture toggle, and UVs | Material asset / MeshRenderer material slot | Runtime material params exist | No serialized material files, normal/specular sampling, transparent materials, or PBR presets yet |
| Textures | Checker texture and WIC texture loading support | Texture assets / Texture importer | Texture creation/loading exists | No texture preview or import settings |
| Shaders | HLSL compile, shared lighting include, F5 reload | Shader assets / shader compiler | Runtime shader reload exists | No shader graph, variant management, error panel |
| Lighting | Lighting model, ambient term, Toon/Rim controls, and directional light list | Light components / material shading model | Directional lighting and first stylized lighting model exist | No light objects in Hierarchy/Inspector, glass/metal material models, or variant management yet |
| Camera | Free perspective camera with keyboard control | Camera component / Scene view camera | Navigable camera exists | No camera object editing or viewport gizmo |
| Input | `InputState` tracks key state | Input Manager / Input System | Keyboard input exists | No action mapping or mouse picking |
| Render Target | Offscreen scene color and depth SRV | RenderTexture / camera target texture | Scene color/depth can be sampled | No target inspector or texture viewer |
| Presentation Pass | Fullscreen scene-color pass | Post-processing volume/effect stack | Render target plumbing exists | User-facing post-process effects are deferred until the Blur/Bloom and depth-effect days |
| Frame Stats | FPS, frame time, draw calls, triangles, passes | Stats overlay / Profiler basics | Basic frame stats visible | No timeline profiler or GPU timing view |
| Wireframe | F1 and Inspector View Debug toggle wireframe raster state | Scene view draw mode | Basic diagnostic mode exists | No shaded/wire/textured mode menu |
| Screenshot | F9 captures PNG | Game view screenshot / Recorder-like utility | PNG capture exists | No naming UI or capture settings |
| Logging | `logs/y-render.log` and OutputDebugString | Console window / Editor log | Log file exists | No in-app console panel |
| GPU Markers | D3D11 annotation events around passes | Frame Debugger / GPU profiler markers | Pass markers exist | No integrated frame debugger |
| Project Config | `config/y-render.ini` controls startup basics | Project Settings / Player Settings | Startup config exists | No in-editor settings UI |
| Demo Switching | Primitive scene / OBJ scene switch | Sample scenes / open scene | Demo scene selection exists | No scene file browser or scene save/load |
| Verification | Debug/Release verify script | Build validation / CI checks | Local build verification exists | No automated visual regression tests |

## Priority Order Toward a Unity-Like Editing Loop

| Priority | Feature | Unity Analogy | Why It Comes Next |
| --- | --- | --- | --- |
| 1 | Scene save/load | Scene asset | Makes Inspector edits persistent |
| 2 | Create/delete/duplicate objects | Hierarchy context actions | Turns the object panel from viewer into editor |
| 3 | Transform gizmo | Move/Rotate/Scale tools | Makes viewport editing practical |
| 4 | Mouse picking | Scene view selection | Connects viewport and Hierarchy selection |
| 5 | Asset filesystem scan | Project panel | Replaces static asset lists with real project assets |
| 6 | Material files | Material assets | Separates reusable materials from per-object values |
| 7 | Texture/depth preview | Inspector / Frame Debugger | Speeds up shader and post-process debugging |
| 8 | In-app console | Console window | Surfaces shader/import/runtime errors without leaving app |
