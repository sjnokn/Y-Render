# Dissolve Effect Collection

The default remains the original basic noise dissolve. The Inspector's `Effects > Dissolve Effects > Type` control switches between eight presets without changing the active lighting model.

| Type | Mask / presentation | Main controls | Video reference |
| --- | --- | --- | --- |
| Basic Noise (default) | Triplanar procedural noise and a two-tone edge | progress, noise scale, edge | [Terra 五分钟小教程：Unity 溶解材质 shader](https://www.bilibili.com/video/BV116nEz8E3t/) |
| Height / Directional | Model-local directional gradient with a noisy boundary | direction, boundary noise | [Unity 特效基础练习：轴向溶解](https://www.bilibili.com/video/BV1CY4y177RX/) |
| Local Sphere | Distance field expanding from a movable model-local center | sphere center, sphere noise | [UE4：简单的自定义形状位置溶解 Part.1](https://www.bilibili.com/video/BV13L411u74m/) |
| Incineration / Ash | One model-local burn field drives clipping, scorched heat bands, fire tongues, fine ash, and closed irregular charcoal crumbs. Crumbs begin on adaptively subdivided source faces, briefly remain attached, then detach under drag, buoyancy, quaternion rotation, and restrained curl turbulence. Local-space simulation keeps every layer aligned while the preview model rotates | direction, boundary noise, edge colors, particle rate/size/lifetime/wind | [Houdini 花瓣燃烧消散教程](https://www.bilibili.com/video/BV12q421w7R2/) |
| FlowMap Energy | Animated procedural vector flow distorts a triplanar field; cyan dual edge pulses along the flow | flow distortion, speed | [ASE + FlowMap 流体效果（合集含鬼神消失）](https://www.bilibili.com/video/BV183411b7JV/) |
| Pixelated | Stable world-space 3D cells combine directional progression with coherent cell noise; the boundary uses white, cyan, and blue-violet whole-cell bands | world pixel size, direction, boundary noise | [像素化消散 shader 原效果](https://www.bilibili.com/video/BV1K54y1P7ua/) |
| Edge Particles / Ash | Directional surface mask plus CPU particles sampled near the moving boundary and drawn as ember/ash point sprites | rate, size, lifetime, wind | [UE5：让粒子从消散边缘发射出来](https://www.bilibili.com/video/BV1xG4y1G7Jw/) |
| Smoke | Directional mask plus soft point sprites that rise, expand, drift, and fade | rate, size, lifetime, wind | [Shader Graph + VFX Graph 烟雾粒子](https://www.bilibili.com/video/BV1FN411D7V1/) |

All masks are calculated in the fragment shader and use `discard` for removed pixels. Incineration mirrors the same model-local burn field on the CPU, so closed charcoal crumbs, fire tongues, and fine ash originate from the visible boundary instead of a separate height-only emitter. The particle layers are simulated in model-local space and transformed with the current object at render time, preventing turntable motion from leaving old particles behind. Standalone edge ash and smoke modes still sample mesh vertices for point particles. Pause freezes dissolve time and every associated motion layer; reverse playback clears emitted fragments before the model reforms.
