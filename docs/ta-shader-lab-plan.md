# TA Shader Lab Plan

Y-Render is now aimed at technical art and shader learning rather than becoming a small Unity editor. The editor shell remains useful, but every new foundation feature should support shader iteration, render debugging, material authoring, or effect presentation.

The default preview should remain static. Time-based motion belongs inside specific shader demos, such as scrolling UVs, dissolve sweeps, scanlines, or water distortion.

## Foundation Direction

| Area | New Role | Avoid Spending Time On |
| --- | --- | --- |
| Objects | Pick the current shader test object | Prefabs, complex parenting, full scene editing |
| Inspector | Separate object transform, material inputs, lighting controls, and view diagnostics | Full Unity-style component system |
| Shader Lab Views | Preview scene color, depth, textures, and later normals/UVs | General asset browser behavior |
| Toolbar | Reload shaders and switch scene presets | Full editor tool ecosystem |
| Scene | Lightweight demo preset container | Full level editing and serialization first |
| Renderer | Multi-pass shader and render-target playground | General-purpose game engine features |

## Implemented Now

| Feature | Purpose |
| --- | --- |
| Showcase Mode | Starts with a full-viewport presentation, compact HUD, auto-framed orbit camera, and optional turntable instead of the editor layout |
| Demo presets and comparison | Studio Lighting and Toon + Rim presets switch with `Tab`/`1`/`2`; `Space` compares each preset with its baseline |
| Inspector Material tab | Live control of base color, albedo texture, and UVs |
| Inspector Lighting tab | Live control of lighting model, material lighting response, directional lights, and Toon/Rim parameters |
| Inspector Effects tab | Keeps surface effects such as Dissolve + Noise separate from the lighting model |
| Procedural shader dog | Default preview mesh with readable body/head/ear/leg silhouettes for shader effects |
| Shader status display | Shows successful reloads or compile errors after shader reload |
| Shader Lab Views | Previews scene color, depth, and checker/albedo texture SRVs |
| Inspector View Debug tab | Holds viewport-only switches such as wireframe without crowding the toolbar |
| Toon Ramp + Rim lighting model | First stylized TA demo path in `Standard.frag` |
| Dissolve + Noise Mask | World-space procedural noise clipping, layered burn edge, animated controls, and a reusable preset |
| Blur + Bloom | Bright-pass extraction, separable blur, composite pass, and live threshold/strength controls |
| Corrected basic lighting | Uses an inverse-transpose normal matrix and keeps Blinn-Phong highlights separate from albedo |

## Day 1 Foundation Baseline

The first implementation day closes the loop around the three foundation areas before new effects are added:

| Area | Day 1 acceptance result |
| --- | --- |
| Model showcase | GLB/glTF/OBJ loading, embedded albedo upload reporting, automatic framing, mouse orbit, zoom, reset, and procedural shader-dog fallback |
| Shader Lab | Object selection, preset reset, material reset, effect bypass, turntable toggle, camera reset, screenshot feedback, shader reload status, and texture/depth diagnostics |
| Material and lighting | Stable Lit/Toon switching, safe zero-length light directions, imported alpha mode labels, multi-light controls, and accurate presentation-pass frame statistics |
| Engineering | OpenGL/GLSL documentation aligned, duplicate character GPU upload removed, Debug/Release verification, and runtime GLB startup check |

The next implementation day starts the first new effect: Dissolve + Noise Mask.

## Eight-Day Vertical-Slice Plan

Each day is one complete small shader project. The next day starts only after the current effect has a working demo, beginner-readable controls, sensible defaults, and a saved presentation preset. Shared renderer systems are built once and then reused; they do not count as unrelated effects that must be started early.

| Day | Core task | What must be added to the earlier prototype | Day-end acceptance |
| --- | --- | --- | --- |
| 1 | Foundation + Toon/Rim | Finish the model showcase, Shader Lab, basic lighting, and Toon + Rim organization; remove confusing effect/lighting labels; keep the fresh showcase environment as presentation support | Stable OpenGL/GLSL baseline with model, material, lighting, camera, debug views, and two lighting demos |
| 2 | Dissolve + Noise | Finish the core threshold mask with world-space noise, natural burn layers, thin bright edge, animation controls, and a clear Effects tab | A reusable Dissolve preset that works on the character and fallback mesh; this is the current day and its core is already working |
| 3 | Blur + Bloom | Add bright-pass extraction and separable blur; connect Bloom to the dissolve edge and tune exposure so the edge is luminous rather than a flat neon stripe | Dissolve has a polished glow preset; Bloom becomes a shared post-process tool for later effects |
| 4 | Depth Fog + Depth Outline | Linearize depth, add distance fog and a controllable depth outline; expose depth preview and outline controls | A complete fog/outline preset plus a reliable depth-debug view |
| 5 | Heat Haze + Refraction | Sample scene color, build a distortion mask, and add animated heat/water distortion with safe screen-edge handling | A complete refraction preset that can be switched off without changing the base material |
| 6 | Normal Map + Specular Map | Add tangent data and real normal/specular texture sampling; provide clear material slots and a neutral fallback | A complete detail-lighting preset that visibly changes surface relief and highlights |
| 7 | Hologram + Scanline | Add time-driven transparency/edge glow, scanlines, flicker, and a stable hologram blend mode | A complete hologram preset with readable controls and no dependency on the dissolve demo |
| 8 | Shadow Map + Debug View | Add a light-space shadow pass, shadow texture preview, bias controls, and diagnostics for acne/peter-panning | A complete shadow demo and a final debug view showing the important render passes |

### Beginner-friendly completion rule

For every effect, use the same three checkpoints:

1. **Core works** — the visual change is visible and can be driven by a parameter.
2. **Presentation is polished** — colors, thresholds, animation, and defaults look intentional on the main model and fallback mesh.
3. **Preset is frozen** — the effect has a named demo, a reset button, a short Chinese explanation, and does not need to be changed while the next effect is built.

Bloom, depth sampling, scene-color sampling, and tangent data are shared renderer capabilities. They are implemented when their first effect needs them and then reused by later effects; they are not separate half-finished visual demos.

## Book-Inspired Feature Order

| Order | Feature | Required Base |
| --- | --- | --- |
| 1 | Toon Ramp + Rim | Material parameters, lighting controls |
| 2 | Dissolve + Noise Mask | Texture slots, threshold/smoothness controls | Day 2 complete; Bloom enhancement belongs to Day 3 |
| 3 | Blur / Bloom | Multi-pass render targets and pass preview |
| 4 | Depth Fog / Depth Outline | Depth linearization and depth preview |
| 5 | Heat Haze / Refraction | Scene color sampling and distortion texture |
| 6 | Normal Map / Specular Map | Tangent data and texture channel preview |
| 7 | Hologram / Scanline | Time-driven material parameters |
| 8 | Shadow Map Debug | Light-space pass and shadow texture preview |
