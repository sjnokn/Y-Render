# Y-Render 底层技术栈切换重构计划(最终版)

> 本计划由"草稿 + 三份对抗式审查(阶段顺序/数学正确性/MSBuild 可行性)"合并而成,所有承重事实均已对照源码二次核验。可直接作为动手蓝图。

---

## 1. 目标与原则

### 1.1 目标技术栈

| 维度 | 现状 | 目标 |
|---|---|---|
| 平台/窗口/输入 | 手写 Win32(WNDCLASSEX/WndProc/PIXELFORMATDESCRIPTOR) | **GLFW 3.3.8**(源码内编,版本钉死) |
| GL 扩展加载 | 手写 `src/Renderer/OpenGL.{h,cpp}`(40 个 PFNGL* extern 指针 + GetGlProc,外加手定义 `GL_UNPACK_ROW_LENGTH`/`GL_STREAM_DRAW`/`GL_ARRAY_BUFFER`/`GL_TEXTURE0`/`GL_CLAMP_TO_EDGE`/`GL_FRAMEBUFFER` 等 enum,因只链 opengl32 1.1 桩) | **GLAD 2**(gl=3.3 core,生成产物 vendored,须含 imgui 用到的全部 enum/扩展) |
| 数学库 | DirectXMath(行向量+行主序) | **GLM**(列向量+列主序,header-only) |
| UI 后端 | imgui_impl_win32 + 自改版 imgui_impl_opengl3(`BackendRendererName="imgui_impl_opengl3_minimal"`,挂 `YRender::` 指针,`#include "Renderer/OpenGL.h"`) | imgui_impl_glfw + imgui_impl_opengl3(改走 glad) |
| 纹理解码/截图编码 | WIC(wincodec.lib + ComPtr + `CoInitializeEx`) | **stb_image / stb_image_write**(header-only) |
| 模型解析 | 手写 GLB chunk walker + 自研 JSON 扫描器(~400 行) | **tinygltf 2.9.x** + nlohmann/json |
| 构建系统 | .vcxproj + build.ps1(MSBuild v143,无 CMake) | 不变,仅扩 external/ 与 IncludePath |

**ImGui 版本锚点(已记录)**:仓库 `external/imgui/` 为完整 **ImGui 1.92.9 WIP** 树,含全套 backend(`imgui_impl_glfw.{cpp,h}`、`imgui_impl_opengl3.{cpp,h,loader.h}`、`imgui_impl_win32.{cpp,h}` 均已 vendored,非新增)。后续工作量只是把已存在的文件加入 vcxproj `ClCompile`,以及对 `imgui_impl_opengl3.cpp` 做定向修改。

### 1.2 原则

1. **视觉/功能等价优先**:每阶段验收必须落盘客观基线(三角形数/world 矩阵 16 float/boundsMin/Max/截图哈希)与上一阶段对拍,肉眼仅作回归确认。
2. **按真实耦合度切分提交,不照搬提示**:
   - 凡存在编译/链接强耦合的改动(GLFW+GLAD+删 OpenGL.h+ImGui 后端),内部按真实依赖再拆可独立编译+运行+对拍的子步骤(Phase 3 的 3a/3b/3c);
   - 凡存在类型耦合的根类型迁移(`SceneTypes.h` 被所有消费者按值持有),整簇原子提交,整簇编译过后才逐场景对拍(Phase 4)。
3. **不引入 CMake/vcpkg/NuGet**:全部走 `external/` 目录 vendoring,与现有 `external/imgui` 源码内编风格一致。
4. **数学迁移风险最高、放最后**:Phase 4 必须以 Phase 2 验收快照为视觉基线锚点。
5. **重写即重写,不夹带**:Phase 2 重写 MeshFactory 时只做"DX 写到能稳定对拍的形态",不夹带任何"顺便优化"的数学改动(数学翻转全部留给 Phase 4,避免双重改写风险)。

### 1.3 环境约束(不变量)

- Windows 10 x64,Visual Studio 2022,PlatformToolset v143,C++17,`/utf-8`,`ConformanceMode=true`,WarningLevel `/W4`(未开 `/WX`),`SDLCheck=true`。
- RuntimeLibrary:Debug=`/MDd`,Release=`/MD`。**决定 GLFW 必须源码内编**(官方预编译 glfw3.lib 仅 /MD)。**源码内编禁止给 GLFW `.c` 单独设 RuntimeLibrary**(须继承全局,否则 LNK2038)。
- SubSystem=Windows(无控制台黑窗)。
- **入口函数决策(已闭合)**:**保留 `wWinMain`** 为首选。零入口/链接改动,`SubSystem` 不动,无需 `<EntryPointSymbol>`;GLFW 在 SubSystem=Windows + wWinMain 下正常工作,`glfwInit` 无需特殊处理。
- 新增 `<LanguageStandard_C>stdc11</LanguageStandard_C>` 字段(放共享 ItemDefinitionGroup),让 GLFW/glad 的 `.c` 按 C11 编译(回退用 `/TP` 有坑:C89 结构触发 C2440)。

---

## 2. 依赖引入总策略(vendoring)

### 2.1 external/ 目录目标布局(已修正 imgui 事实)

```
external/
├── imgui/                    (已存在,完整 1.92.9 WIP 源码内编树)
│   └── backends/             (全套后端均已 vendored,下列文件均非新增)
│       ├── imgui_impl_glfw.{cpp,h}        ← 已存在,仅需加入 ClCompile(Phase 3a)
│       ├── imgui_impl_opengl3.{cpp,h}     ← 已存在,内容做定向修改走 glad(Phase 3c)
│       ├── imgui_impl_opengl3_loader.h    ← 已存在,留树中不参与构建(走 glad 路线 a)
│       └── imgui_impl_win32.{cpp,h}       ← 已存在,Phase 3b 从 ClCompile 移除
├── glfw/                     ← 新增,【源码内编】(版本 3.3.8 钉死)
│   ├── include/GLFW/glfw3.h
│   ├── include/GLFW/glfw3native.h
│   └── src/  (见 §3.3.4 枚举式清单 + glfw_config.h)
├── glad/                     ← 新增,在线生成
│   ├── include/glad/gl.h
│   ├── include/KHR/khrplatform.h
│   └── src/glad.c
├── glm/                      ← 新增,纯头树(<glm/glm.hpp>)
├── stb/                      ← 新增,纯头 + 单实现 TU
│   ├── stb_image.h
│   ├── stb_image_write.h
│   └── stb_image_impl.cpp   (唯一 STB_IMAGE_IMPLEMENTATION 持有者)
├── json/                     ← 新增(tinygltf 的 JSON 后端)
│   └── json.hpp             (nlohmann/json 单头,~25K 行,可选 PCH)
└── tinygltf/                 ← 新增,纯头 + 单实现 TU
    ├── tinygltf.h
    └── tinygltf.cpp          (唯一 TINYGLTF_IMPLEMENTATION 持有者)
```

### 2.2 各库引入决策

| 库 | 方式 | 理由 |
|---|---|---|
| **GLFW** | **源码内编(3.3.8)** | 官方预编译 `glfw3.lib` 仅 /MD,Debug(/MDd)会 LNK2038。源码内编自动跟随主工程 CRT。 |
| **GLAD** | 在线生成(gl=3.3 core)+ glad.c 入 vcxproj | 见 §3.3.1。生成参数必须包含 `imgui_impl_opengl3.cpp` 用到的全部 enum/扩展,仅 2 个扩展不够。 |
| **GLM** | 纯头,仅加 IncludePath | header-only。**禁止** define `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES`/`GLM_FORCE_SSE2`(会 16 字节对齐填充 vec3/vec4,破坏 Vertex POD 与 ImGui 取址)。 |
| **stb_image / stb_image_write** | 纯头 + 唯一 TU 持有两个 IMPLEMENTATION | 全局只能定义一次,避免 LNK2005。 |
| **tinygltf** | 纯头 + 唯一 TU;`TINYGLTF_NO_STB_IMAGE` + `TINYGLTF_NO_STB_IMAGE_WRITE` | **理由修正**:tinygltf.h 本来就不定义 STB_IMAGE_IMPLEMENTATION;加了这两个宏后 tinygltf 既不 include 也不调用 stb,`LoadImageData`/`WriteImageData` 被编译剔除,0 个 stb 符号依赖即可编译链接。目的是**避免 tinygltf 自带 stb 调用路径与本工程解码职责重复**,而非"确立 canonical owner"。功能侧也独立:`MeshFactory::ParseEmbeddedImages` 只把 glTF image 原始字节塞进 `EmbeddedImage.bytes`,解码在 ResourceManager 的 WIC/stb 路径。 |
| **nlohmann/json** | 纯头,随 tinygltf vendoring | ~25K 行,每 include tinygltf.h 的 TU 重编 +5-15s,可选 PCH。备选:picojson + `TINYGLTF_NO_EXTERNAL_JSON`。 |

### 2.3 vcxproj 改动汇总

vcxproj 锚点行号:38(IncludePath)/44,59(Preprocessor,Debug/Release)/52,68(链接库)/35-39(无 Condition 的 PropertyGroup,改一次两配置生效)/72-94(ClCompile)/95-118(ClInclude)。

1. **IncludePath(第 38 行)**,最终值追加:`$(ProjectDir)external;$(ProjectDir)external\glfw\include;$(ProjectDir)external\glad\include;$(ProjectDir)external\stb;$(ProjectDir)external\tinygltf;$(ProjectDir)external\json`(GLM 头路径是 `<glm/glm.hpp>`,故加 `external` 而非 `external\glm`)。该 PropertyGroup 无 Condition,**改一次 Debug/Release 同时生效**。
2. **ClCompile** 增:`external\glad\src\glad.c`、**GLFW src 枚举式 12 个 `.c`(见 §3.3.4 清单)**、`external\stb\stb_image_impl.cpp`、`external\tinygltf\tinygltf.cpp`、`external\imgui\backends\imgui_impl_glfw.cpp`(**已 vendored,非新增文件,仅首次加入 ClCompile**)。
3. **ClCompile** 删:`src\Renderer\OpenGL.cpp`(Phase 3c)、`external\imgui\backends\imgui_impl_win32.cpp`(Phase 3b)。
4. **ClInclude** 同步增删。
5. **第三方 C 源降噪(per-item)**:给 `external\glad\src\glad.c`、12 个 GLFW `.c`、`external\tinygltf\tinygltf.cpp`、`external\stb\stb_image_impl.cpp` 单独设 per-item Preprocessor + 警告压制。**必须加 `_CRT_SECURE_NO_WARNINGS`**(C4996):GLFW/tinygltf/stb/json 用旧 CRT,在 `SDLCheck=true` + `/W4` 下刷上千条 C4996。
6. **PreprocessorDefinitions**(44/59,两配置都加,全局):
   `TINYGLTF_NO_STB_IMAGE;TINYGLTF_NO_STB_IMAGE_WRITE;TINYGLTF_USE_CPP14;`
   - **全局 vs IMPLEMENTATION 表述统一(关键)**:`TINYGLTF_NO_STB_IMAGE`/`_WRITE`/`USE_CPP14` 必须在所有 include tinygltf.h 的 TU 可见(否则 ODR 风险:cpp 走 C++14 路径、其它 TU 看 C++11 路径,`make_unique` 等处 ODR 不一致)→ 全局 Preprocessor 双配置添加;`TINYGLTF_IMPLEMENTATION` **仅在 `tinygltf.cpp` 内 define**;`STB_IMAGE_IMPLEMENTATION`/`STB_IMAGE_WRITE_IMPLEMENTATION` **仅在 `stb_image_impl.cpp` 内 define**。
7. **新增 `<LanguageStandard_C>stdc11</LanguageStandard_C>`**(共享 ItemDefinitionGroup):让 `.c` 走 C11,替代不可靠的 `/TP` 回退。
8. **链接库**(52/68):`opengl32.lib` 始终保留;`windowscodecs.lib` **仅在 Phase 1 三处 WIC 路径全迁、且 `CoInitializeEx`/`CoUninitialize` 清理后才删**(独立提交);**`ole32.lib` 隐式链接确认保留**(CoInitializeEx 在 `src/YRender.cpp:8/11`,删这两行后 ole32 无消费者,可一并核对)。

### 2.4 per-item XML 元数据范例(改自闭合为带子元素)

当前 `ClCompile` 均为自闭合 `<ClCompile Include="..."/>`。给第三方 `.c` 单独设 WarningLevel/Preprocessor 必须改成展开式。最小范例:

```xml
<ClCompile Include="external\glfw\src\context.c">
  <WarningLevel>Level3</WarningLevel>
  <PreprocessorDefinitions>_CRT_SECURE_NO_WARNINGS;NOMINMAX;WIN32_LEAN_AND_MEAN;%(PreprocessorDefinitions)</PreprocessorDefinitions>
  <DisableSpecificWarnings>4996;4244;4267;4189</DisableSpecificWarnings>
  <LanguageStandard_C>stdc11</LanguageStandard_C>
</ClCompile>
```

`tinygltf.cpp`/`stb_image_impl.cpp` 同构(它们是 `.cpp`,不带 `LanguageStandard_C`,但保留 `_CRT_SECURE_NO_WARNINGS;NOMINMAX;WIN32_LEAN_AND_MEAN` 与 DisableSpecificWarnings)。注意 `%(PreprocessorDefinitions)` 末尾追加,确保继承全局 `TINYGLTF_*`。

### 2.5 windowscodecs.lib 与 COM 清理

**能移除,有时序前提**。WIC 三处路径:`ResourceManager::LoadTextureWic`、`LoadTextureWicFromMemory`、`RenderDevice::CaptureBackBufferPng`。grep 已确认:`#include <wincodec.h>`/`<wrl/client.h>` 与 `IWIC*` 仅这 3 处;`CoInitializeEx`/`CoUninitialize` 仅 `src/YRender.cpp:8/11` 一处(WIC 是唯一 COM 消费者)。流程:

1. 三处 WIC 路径全迁到 stb(Phase 1 主体);
2. 删 `src/YRender.cpp:8` `CoInitializeEx(...)` 与 `:11` `CoUninitialize();`(Phase 1 收尾);
3. 删 `windowscodecs.lib`(52/68)+ 核对 `ole32.lib` 是否还有消费者,若无一并删(Phase 1 独立提交)。

### 2.6 build.ps1 / verify.ps1

- build.ps1:msbuild 不变,加依赖就绪性 Test-Path 校验清单(缺失抛清晰错误):
  - `external\glfw\include\GLFW\glfw3.h`
  - `external\glad\include\glad\gl.h` + `external\glad\include\KHR\khrplatform.h`
  - `external\stb\stb_image.h` + `external\stb\stb_image_write.h`
  - `external\tinygltf\tinygltf.h`
  - `external\json\json.hpp`
  - `external\imgui\backends\imgui_impl_glfw.h`(**校验已存在,非新增**)
- verify.ps1:保持 Debug→Release 双构建;不加 headless smoke(GLFW 需显示,CI 易假阴)。

---

## 3. 分阶段工作分解

### 3.0 阶段依赖图(已修正)

```
                          Phase 0 (Vendoring 骨架:落文件 + IncludePath,不触发行为变化)
                                       │
       ┌───────────────────────────────┼───────────────────────────────┐
       ▼                               ▼                               ▼
   Phase 1 (stb)         Phase 2 (tinygltf) ── 落盘黄金 world/bounds     Phase 3 (GLFW+GLAD+ImGui+删OpenGL.h)
   独立、低风险            独立、仅依赖 Phase 0            │              内部拆 3a/3b/3c,见 §3.3
                          (与 Phase 1 无依赖)             │              │
                                                          │              │
                              Phase 4 视觉基线 = Phase 2 验收快照 ◀────── 阻塞 ──┘
                                                          │
                                                          ▼
                                              Phase 4 (GLM + shader 上传统一)
                                              根类型 + 全消费者原子簇(同 commit)
```

**排序理由(已修正)**:
- **Phase 1/2/3 三者互相独立**,均仅依赖 Phase 0。
  - 修正原"Phase 2 依赖 Phase 1 确立 canonical owner"的错误:`TINYGLTF_NO_STB_IMAGE` + `_WRITE` 后 tinygltf 0 个 stb 符号依赖(§2.2)。Phase 2 仅依赖 Phase 0,可与 Phase 1 完全并行。
- **Phase 3 内部拆 3a/3b/3c**(原"必须单 commit"过强):`OpenGL.h` 的 `LoadOpenGLFunctions` 内部用 `wglGetProcAddress`,而 GLFW 在 Windows 底层就是用 WGL 建上下文(`wgl_context.c`),`glfwMakeContextCurrent` 后 `wglGetProcAddress` 仍可用——`OpenGL.h` 可作为过渡 shim 在 GLFW 上下文上工作。`imgui_impl_opengl3.cpp` 是完全定制最小文件,与平台后端零耦合。故可拆 3 步,每步独立可编译+运行+视觉对拍。
- **Phase 4 放最后且内部是原子簇**:`SceneTypes.h` 是根类型(`Vertex{XMFLOAT3...}`、`Transform`、`Material`、`Mesh` 被所有消费者按值持有),一旦 `Vertex::position` 改 `glm::vec3`,所有消费者同时破裂(`XMFLOAT3`↔`glm::vec3` 无隐式转换),不可能"先改 SceneTypes.h→rebuild→运行"。Phase 4 = `SceneTypes.h` + 全部消费者类型迁移同 commit,整簇编译过后才逐场景对拍。
- **Phase 4 视觉基线锚点**:Phase 2 是语义重写(修复 `hasParent` 启发式丢根节点 bug + 补回 `componentType!=5126` 整 primitive 丢弃),落地后三角形数/world 矩阵/bounds 会变。Phase 4 拿新基线对拍才能区分 GLM bug vs Phase 2 偏差。**Phase 2 落地时立即落盘 world 矩阵/bounds 黄金值作为 Phase 4 锚点**(§3.2)。
- **shader 维度**:8 个 .vert/.frag 零改动,但其上传统一(uViewProj 双转置冲突)与 GLM 强耦合,并入 Phase 4。

### 3.1 Phase 1:stb_image + stb_image_write(去 WIC,低风险独立)

**目标**:用 stb_image 替换 WIC 解码、stb_image_write 替换 WIC 截图编码,移除 `windowscodecs.lib` 与 COM 初始化。完全独立,无 GL/窗口/数学耦合。

**文件级改动**:

| 文件 | 操作 | 改动 |
|---|---|---|
| `external/stb/stb_image.h`,`stb_image_write.h`,`stb_image_impl.cpp` | new | 唯一 IMPLEMENTATION TU。`#define STB_IMAGE_IMPLEMENTATION`/`STB_IMAGE_WRITE_IMPLEMENTATION`/`STBI_WINDOWS_UTF8`/`STBI_FAILURE_USERMSG`。**`#pragma warning(push,0)` 必须紧贴 include 之前**;`stb_image_write.h` 旧版无自压制,在它之前也 push。 |
| `src/Assets/ResourceManager.h` | modify | 删 `#include <wincodec.h>`;签名不变 |
| `src/Assets/ResourceManager.cpp` | replace | 删 `<wrl/client.h>`、`LoadTextureWic`(152-175)与 `LoadTextureWicFromMemory`(177-209) COM 流程,改 `stbi_load`/`stbi_load_from_memory(STBI_rgb_a)` + `CreateTextureFromRgba` + `stbi_image_free`;`Initialize()` 加 `stbi_set_flip_vertically_on_load(false)`(**正确性锚点**);`UploadEmbeddedTextures` 的 `catch(...)` 收窄为 `catch(const std::exception&)` + LogError |
| `src/Renderer/RenderDevice.cpp` | replace | `CaptureBackBufferPng`(141-209)删全部 IWIC* 编码,改 `stbi_write_png`;删 `<wincodec.h>`/`<wrl/client.h>`,加 `<stb_image_write.h>` |
| `src/YRender.cpp` | modify | **删 `:8 CoInitializeEx(...)` 与 `:11 CoUninitialize();`**(WIC 是唯一 COM 消费者,grep 已确认仅此一处) |
| `YRender.vcxproj` | modify | IncludePath 加 `external\stb`;ClCompile 加 `stb_image_impl.cpp`(per-item 配置见 §2.4);三处 WIC + COM 清理后,独立提交删 `windowscodecs.lib` + 核对 `ole32.lib` |

**验证**:Debug+Release 双构建;加载 character.glb 确认纹理;Y 翻转基线对拍(上下有方向标记的 PNG,迁移前后朝向一致);截图产出一致;中文路径靠 `STBI_WINDOWS_UTF8`;**落盘基线**(纹理哈希、截图哈希)。

**依赖**:仅 Phase 0。与 Phase 2/3 无交集。

### 3.2 Phase 2:tinygltf(替换手写 GLB/JSON 解析,独立)

**目标**:用 tinygltf 替换 `MeshFactory.cpp` 的 GLB chunk walker + 自研 JSON 扫描器(~400 行)。**MeshFactory.h 公共签名零改动**。**不夹带任何数学优化**(仅做"DX 写到能稳定对拍"的形态,数学翻转全部留给 Phase 4)。

**文件级改动**:

| 文件 | 操作 | 改动 |
|---|---|---|
| `external/tinygltf/tinygltf.{h,cpp}`、`external/json/json.hpp` | new | tinygltf.cpp 唯一 TU 内 define:`TINYGLTF_IMPLEMENTATION`(仅此 TU);全局 Preprocessor 加 `TINYGLTF_NO_STB_IMAGE`/`TINYGLTF_NO_STB_IMAGE_WRITE`/`TINYGLTF_USE_CPP14`(§2.3) |
| `src/Assets/MeshFactory.cpp` | replace | **主战场**。删 GltfAccessor/BufferView/Node 镜像 + JSON 扫描器 + LoadGlb chunk walker + Parse*。重写 LoadGltf:tinygltf::TinyGLTFLoader,LoadBinaryFromFile(.glb)/LoadASCIIFromFile(.gltf);遍历 `model.scenes[defaultScene].nodes`(**修复 hasParent 启发式丢根节点 bug**);ComputeNodeWorld(保留行向量 `world=local*parentWorld`,**不翻数学序,留给 Phase 4**);AppendGltfPrimitive 支持 componentType 5120/5121/5122/5123 含 normalized。**保留** LoadObj/GenerateNormals/UpdateMeshBounds/NormalizeMeshForPreview/Create*Mesh |
| `src/Assets/MeshFactory.h` | modify | 零功能改动 |
| `YRender.vcxproj` | modify | IncludePath 加 tinygltf/json;ClCompile 加 tinygltf.cpp(per-item `_CRT_SECURE_NO_WARNINGS`) |

**验证(含黄金值落盘)**:
- 先单独编 tinygltf.cpp(无 LNK2005/ODD);
- 加载 character.glb 比对迁移前后三角形数/submesh 数/boundsMin/Max 一致;
- **重点查静默丢面**(旧对 componentType!=5126 整 primitive 丢弃,新应补回);
- **黄金值落盘(Phase 4 锚点)**:在 Phase 2 验收时,对每个 glTF 场景 Log 输出并落盘到 `docs/baselines/phase2-<scene>.txt`:每个节点的 world 矩阵 16 float + 整体 boundsMin/Max + 三角形数。**显式声明 Phase 4 视觉基线 = Phase 2 验收快照**。
- 角色姿态/baseColor/法线肉眼一致;sample.obj(LoadObj)仍可用。

**依赖**:仅 Phase 0(不依赖 Phase 1)。与 GL/GLFW/GLM 无交集,可与 Phase 1/3 并行分支。

### 3.3 Phase 3:GLFW + GLAD + ImGui-glfw + 删除 OpenGL.h(原子平台地基,拆 3a/3b/3c)

**目标**:GLFW 接管窗口/上下文/事件链;GLAD 取代手写 GL loader;ImGui 后端 win32→glfw;删除 `src/Renderer/OpenGL.{h,cpp}`。按真实依赖拆三步,每步独立可编译+运行+视觉对拍。若团队坚持单提交,必须在单 commit 内对 3a/3b/3c 三组各做 rebuild + 视觉对拍 + 输入回归。

**OpenGL.h 的 5 个消费者清单(Phase 3c 切 glad 时必须逐个改)**:
1. `src/Renderer/RendererApp.cpp`
2. `src/Renderer/RenderDevice.cpp`
3. `src/Renderer/BurnFragmentSystem.cpp`
4. `src/Renderer/DissolveParticleSystem.cpp`
5. `src/Assets/ResourceManager.cpp`

外加 `external/imgui/backends/imgui_impl_opengl3.cpp`(定制版,走路线 a,见 §3.3.2)。

#### 3.3.1 GLAD 集成设计

- **生成参数**(glad2 在线):gl=3.3,core,loader 运行期用 `glfwGetProcAddress`(生成时 WGL)。
- **必须包含 imgui_impl_opengl3.cpp 用到的全部 enum/扩展**(M3 关键):原计划只列 `WGL_EXT_swap_control`、`GL_ARB_draw_elements_base_vertex` 不够。`OpenGL.h` 除了 40 指针还手定义 `GL_UNPACK_ROW_LENGTH`/`GL_STREAM_DRAW`/`GL_ARRAY_BUFFER`/`GL_TEXTURE0`/`GL_CLAMP_TO_EDGE`/`GL_FRAMEBUFFER` 等(因只链 opengl32 1.1 桩),`imgui_impl_opengl3.cpp` 依赖这些,删 OpenGL.h 后必须由 glad.h 提供。
  - **生成约束**:glad2 生成参数补充"必须包含 imgui_impl_opengl3.cpp 用到的全部 enum";**或**在 Phase 3c 验证加一道:"先单独编 `imgui_impl_opengl3.cpp`,枚举缺失 glad 符号再重生 glad"。
- **namespace 问题**:GLAD 生成全局符号 `glad_glXxx` + 宏 `#define glXxx glad_glXxx`。所有消费者 `#include "Renderer/OpenGL.h"` → `<glad/gl.h>`,裸调 `glXxx`。
- **上下文创建**:core profile 经 `glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR/MINOR=3.3, GLFW_OPENGL_PROFILE=CORE)`(取代手写 wglCreateContextAttribsARB)。
- **加载时序(硬约束)**:`glfwMakeContextCurrent` **必须先于** `gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)`,否则指针 null,首个 glGenFramebuffers 即崩。
- **gladLoadGLLoader 返回值断言(M7/O7 补)**:加载后 `assert(gladLoadGLLoader(...) != 0)`;最小 sanity 断言 `glGenFramebuffers != nullptr`(或尝试 `glGetIntegerv` 取一个非 0 值),失败立即报错而非空指针崩在渲染中段。
- **include 顺序(硬约束)**:每个用到 GL 的 TU,`#include <glad/gl.h>` 必须在 `<GLFW/glfw3.h>`/`<windows.h>`/`<gl/GL.h>` **之前**,否则系统 GL 头抢注同名符号链到 opengl32 1.1 桩。

#### 3.3.2 imgui_impl_opengl3.cpp 路线选择(已定:走路线 a)

两条互斥路线:
- **(a) 保留自改 cpp(选定)**:`#include "Renderer/OpenGL.h"` → `<glad/gl.h>`;所有 `YRender::glXxx` → 裸 `glXxx`;`imgui_impl_opengl3_loader.h` **留树中不参与构建**。
- (b) 回 stock 让 cpp include `imgui_impl_opengl3_loader.h` —— 与 glad 同 TU 必出重定义,**不可行**。

**显式走 (a)**。`imgui_impl_glfw.cpp` 不依赖任何 loader 宏(与 opengl3 后端解耦,S8),Phase 3a 加入即可编译。

#### 3.3.3 Phase 3a:vendoring 编译(落库 + 链接通过,行为同 Phase 0)

- 落 `external/glfw`(整树,3.3.8)+ `external/glad`(整树)+ `glfw_config.h`;
- **imgui_impl_glfw.{cpp,h} 已 vendored,仅首次加入 ClCompile**;`imgui_impl_opengl3_loader.h` 留树不入构建;
- 旧 Win32 路径、`OpenGL.h` 全不动;
- **glfw_config.h 内容**(M5):`#define _GLFW_WIN32`、`#define _GLFW_USE_OPENGL`、**保险加 `#define _GLFW_WGL`**(3.3 冗余但不冲突)、**可选 `#define _GLFW_USE_HYBRID_HPG`**(笔记本默认走 dGPU)。
- per-item 降噪:`glad.c` + 12 个 GLFW `.c` + tinygltf.cpp + stb_image_impl.cpp 加 `_CRT_SECURE_NO_WARNINGS;NOMINMAX;WIN32_LEAN_AND_MEAN` + DisableSpecificWarnings(§2.4)。

**验证**:Debug+Release 链接通过(无 LNK2019/LNK2038/LNK2005);运行一次,行为同 Phase 0(窗口仍是旧 Win32 路径创建,新代码未接入渲染)。

#### 3.3.4 GLFW src 枚举式清单(12 个,M4 修正)

vcxproj 不支持 glob,**必须逐个枚举**,改"排除式"为"枚举式"。Win32+WGL 应编 **12 个**:

**通用 5**:`context.c`、`init.c`、`input.c`、`monitor.c`、`window.c`。
**Win32 平台 6**:`win32_init.c`、`win32_joystick.c`、`win32_monitor.c`、`win32_thread.c`、`win32_time.c`、`win32_window.c`。
**WGL 1**:`wgl_context.c`。

**必须排除**(不加入构建):
`null_*.c`、`osmesa_context.c`、`egl_context.c`、`vulkan.c`、`posix_thread.c`、`posix_time.c`、`x11_*`、`cocoa_*.mm`(**Objective-C++,MSBuild 编不了**)、`wayland_*`、`wl_*`、`linux_joystick.c`、`xkb_unicode.c`、`mir_*`。

#### 3.3.5 Phase 3b:窗口/上下文/事件/ImGui 平台后端(GLFW 接管,保留 OpenGL.h 作过渡 shim)

**关键事实**:`OpenGL.h` 的 `LoadOpenGLFunctions` 内部用 `wglGetProcAddress`,GLFW 在 Windows 用 WGL 建上下文(`wgl_context.c`),`glfwMakeContextCurrent` 后 `wglGetProcAddress` 仍可用——**OpenGL.h 可作过渡 shim,本步不删**。

| 文件 | 操作 | 改动 |
|---|---|---|
| `src/YRender.cpp` | modify | 保留 `wWinMain`;`app.Run(instance,showCommand)` → `app.Run()`;`glfwSetErrorCallback`+`glfwInit()` 包裹,Run 后 `glfwTerminate()` |
| `src/Renderer/RenderDevice.h` | modify | 删 `HWND m_hwnd`/`HDC m_hdc`/`HGLRC m_glrc`;`Initialize(HWND,UINT,UINT)` → `Initialize(GLFWwindow*,int,int)`;`IsInitialized()` 改 `m_window!=nullptr` |
| `src/Renderer/RenderDevice.cpp` | replace | Initialize 删 GetDC/PFD/ChoosePixelFormat/SetPixelFormat/wglCreateContext/wglMakeCurrent,改 `glfwMakeContextCurrent`;**保留 `LoadOpenGLFunctions()`(过渡 shim)**;Present:`SwapBuffers(m_hdc)`→`glfwSwapBuffers(m_window)`;include 加 `<GLFW/glfw3.h>`(**OpenGL.h 暂留**) |
| `src/Renderer/RendererApp.h` | replace | `HWND m_hwnd` → `GLFWwindow* m_window`;删 WindowProc/HandleMessage/OnKeyPressed(UINT)/OnMouseButton/OnMouseMove/OnMouseWheel/IsPointInPreview(POINT);改 6 个 GLFW 静态回调 trampoline;`Run(HINSTANCE,int)` → `Run()` |
| `src/Renderer/RendererApp.cpp` | replace | (1) CreateAppWindow:`glfwInit`+`glfwWindowHint(core 3.3)`+`glfwCreateWindow`+`glfwSetWindowUserPointer`+`glfwMakeContextCurrent`+`LoadOpenGLFunctions()`;(2) 删 WindowProc,实现 6 回调:Key(PRESS→OnKeyPressed+SetKey(true),RELEASE→SetKey(false),**REPEAT 忽略**)、MouseButton、CursorPos(累加 pendingLookDelta)、Scroll(累加 pendingWheelDelta)、FramebufferSize(→Resize)、WindowClose(`m_running=false`)、WindowFocus(GLFW_FALSE 清空 input);(3) ImGui:InitForOpenGL/NewFrame/Shutdown 改 glfw 后端(imgui_impl_win32→imgui_impl_glfw,vcxproj 同步切换 ClCompile);(4) MainLoop:`PeekMessage`→`while(!glfwWindowShouldClose){glfwPollEvents();...}`;(5) UpdateWindowTitle:`SetWindowTextW`→`glfwSetWindowTitle`(**UTF-8**,WideToUtf8);(6) **删 `glEnable(GL_TEXTURE_2D)`(原 RenderDevice.cpp:58,core 下 GL_INVALID_ENUM)** |
| `src/Core/GlfwInputMap.h` | new | `inline int GlfwKeyToVk(int)`:GLFW 特殊键→VK;可打印键原样(GLFW_KEY_A=='A'==65)。让 InputState/Camera/OnKeyPressed 零改 |
| `src/Core/Input.{h,cpp}` | modify | **硬性必改**:`m_down[256]` → **`m_down[512]`**(或引入 `enum class Key`)。GLFW_KEY_ESCAPE=256/TAB=258/F1=290/LEFT_SHIFT=340 超出 256,下标越界 |
| `YRender.vcxproj` | modify | ClCompile:`imgui_impl_win32.cpp` 移出,`imgui_impl_glfw.cpp` 移入(两者均已 vendored) |

**验证**:窗口由 GLFW 创建;渲染基线逐像素一致(OpenGL.h shim 在 GLFW 上下文工作);输入回归(WASD/方向键/Shift、鼠标拖拽、滚轮、F5/F1/F2/F9/ESC);ImGui 回归(io.WantCapture 门控);垂直同步。

#### 3.3.6 Phase 3c:loader 替换(切 glad,删 OpenGL.h)

| 文件 | 操作 | 改动 |
|---|---|---|
| OpenGL.h 的 **5 个消费者** | replace | `#include "Renderer/OpenGL.h"` → `<glad/gl.h>`(include 顺序在 windows.h/GLFW 之前);`YRender::glXxx` → 裸 `glXxx`(若消费侧用到前缀) |
| `src/Renderer/RenderDevice.cpp` | replace | `LoadOpenGLFunctions()` → `assert(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0)`;最小 sanity:`glGenFramebuffers != nullptr`;`wglSwapIntervalEXT` → `glfwSwapInterval(1)` |
| `external/imgui/backends/imgui_impl_opengl3.cpp` | modify | 走路线 a:`#include "Renderer/OpenGL.h"` → `<glad/gl.h>`;`YRender::glXxx` → 裸 `glXxx`(§3.3.2) |
| `src/Renderer/OpenGL.{h,cpp}` | **delete** | 整文件删除 + vcxproj 移除 |

**验证**:渲染再次逐像素一致;`gladLoadGLLoader` 返回值断言通过;core profile 下无 GL_INVALID_ENUM。

> **core profile 风险提示**:shaders `#version 130` + `in/out`/`texture()` 在 core 合法。**回退预案**:core 不行改 `GLFW_OPENGL_COMPAT_PROFILE`,GLAD 无需重生。

**依赖**:Phase 0。**阻塞** Phase 4。

### 3.4 Phase 4:DirectXMath → GLM + shader 上传统一(根类型 + 全消费者原子簇)

**目标**:数学库整体切换 + 统一三个 uViewProj 上传点。**shader 源码零改动**(全为列向量 `M*v` 语义)。**整簇同 commit**:只有 `SceneTypes.h` + 全部消费者类型迁移一起编译过后,才逐场景对拍。

#### 3.4.1 正确性四立柱

1. **矩阵乘法顺序整体取逆**:DX(行向量 S*R*T)→ GLM(列向量 T*R*S)。Transform `S*R*T`→`T*R*S`;Camera `view*projection`→**`projection*view`(投影在前、视图在后,反直觉易照字面写反,加粗标注)**;MeshFactory TRS `S*R*Q*T`→`T*Q*R*S`;ComputeNodeWorld `local*parent`→`parent*local`。
2. **手系/深度保持 LEFT-handed + [0,1] Z**:投影用 `glm::perspectiveLH_ZO`(绝不用默认 `glm::perspective`);视图 `glm::lookAtLH`(`up=(0,1,0)` 与 `forward=target-eye` 在 LH 下与 `XMMatrixLookAtLH` 同向,无需翻符号)。`Camera::UpdatePosition` 的 `m_position.z = m_target.z - cos(yaw)*hDist` 减号不得改。FOV `XMConvertToRadians(45.0f)` → **`glm::radians(45.0f)`**。`glDepthFunc(GL_LEQUAL)` 不变。
3. **上传统一一套约定**:全部 `glUniformMatrix4fv(loc,1,GL_FALSE,glm::value_ptr(m))`,无手动转置。
4. **法线矩阵——区分两条路径(问题 2 修正,勿一刀切)**:
   - **GPU 上传路径(`RendererApp.cpp:909`,喂 `uniform mat4 uNormalMatrix`)**:必须保持 mat4,用 `glm::transpose(glm::inverse(world))`(`world` 是 `glm::mat4`,对齐 uniform mat4,这才是 `XMMatrixTranspose(XMMatrixInverse(nullptr,world))` 的真等价)。**若用 `glm::mat3` 再 `value_ptr(mat3)` 只指 9 float,而 `glUniformMatrix4fv` 读 16 → 越界读栈垃圾/崩溃**(Standard.vert:9 声明 `uniform mat4 uNormalMatrix`,RendererApp.cpp:911 经 SetMat4 上传 16 float)。
   - **CPU 预烘焙路径(`MeshFactory.cpp:953`,只喂 TransformNormal 用 xyz)**:可用 `glm::transpose(glm::inverse(glm::mat3(world)))`。
   - 两处都显式写清。

#### 3.4.2 uViewProj 双转置:无条件统一(现状已证实为 bug,不保留 transpose)

**承重事实(源码核验)**:
- 三个 shader 的矩阵-向量语义**完全相同**(列向量 `uViewProj * vec4(pos,1)`):`BurnFragment.vert:24`、`DissolveParticle.vert:19`、`Standard.vert:21`。
- `viewProjection` 入参全部来自同一个 `Camera::ViewProjection()`。
- `RendererApp::SetMat4`(1299-1300):`XMStoreFloat4x4(&value, matrix)` 不预转置 + GL_FALSE → 净效果 = 上传矩阵的转置,**Standard 渲染正确**。
- `BurnFragmentSystem.cpp:793` + `DissolveParticleSystem.cpp:608`:`XMStoreFloat4x4(&matrix, XMMatrixTranspose(viewProjection))` 再 GL_FALSE → 净效果 = 两次转置 = 上传未转置的行向量矩阵,**数学上确定错误**。

既然三 shader 语义逐字相同,Burn/Dissolve **没有任何可能"依赖怪癖而正确"**。**原"先肉眼判定、可能保留 transpose"的决策流程作废**。

**决策(无条件统一)**:
- Burn/Dissolve 迁移后**无条件**走 `glUniformMatrix4fv(loc,1,GL_FALSE,glm::value_ptr(m))`(与 Standard 统一),**不保留任何 transpose**;
- 肉眼检查只用于**回归确认**(迁移后粒子/碎片位置应**更正确**,因为修了一个 bug),不是"决定是否保留"的输入。

**客观校验手段(补充)**:迁移后**临时**把粒子系统的 uViewProj 改走 `SetMat4`(Standard 约定)上传,对比位置:
- 若粒子位置**变**了 → 原 Burn/Dissolve 路径确为 bug(与新统一约定一致);
- 若位置**不变** → 分析有遗漏,需复查(三 shader 语义相同的前提下不应出现)。

> **补充背景**:MeshFactory position/normal 在导入时预烘焙到世界空间(966/978 行),运行时 uWorld 对 glTF 物体常接近单位矩阵,验证策略要点明。

#### 3.4.3 原子簇迁移清单(同 commit,整簇编译后才逐场景对拍)

**为什么不叶子优先**:`SceneTypes.h` 的 `Vertex{XMFLOAT3 position}`、`Transform`、`Material`、`Mesh` 被所有消费者**按值持有**。一旦 `Vertex::position` 改 `glm::vec3`,所有消费者同时破裂(`XMFLOAT3`↔`glm::vec3` 无隐式转换)。整簇必须同 commit;整簇编译过后才能逐场景对拍。数学语义翻转(矩阵序/四元数序/双转置)在簇内**逐函数**验证(Log world 16 float 与 Phase 2 黄金值比对)。

整簇文件清单:

| 文件 | 改动要点 |
|---|---|
| `src/Scene/SceneTypes.h` | 删 `<DirectXMath.h>`,加 glm。XMFLOAT2/3/4 → glm::vec2/3/4(~30 处)。Transform `S*R*T` → `translate*R(yawPitchRoll)*scale`(**`yawPitchRoll(yaw,pitch,roll)` 与 `RotationRollPitchYaw(pitch,yaw,roll)` 参数序不同**) |
| `src/Scene/Camera.{h,cpp}` | `XMMATRIX ViewProjection` → `glm::mat4`;**`return glm::perspectiveLH_ZO(...) * glm::lookAtLH(...)`(投影在前、视图在后)**;`XMConvertToRadians(45.0f)` → `glm::radians(45.0f)`;删 `using namespace DirectX`;XMLoad/XMStore/XMVector* → glm 运算符 |
| `src/Scene/Scene.cpp` | XMFLOAT3/4 字面量 → glm;`XM_PI`→`glm::pi<float>()` |
| `src/Assets/MeshFactory.cpp` | **node.matrix(问题 3 修正)**:`XMMatrixSet` 行填 + `XMMatrixTranspose` → `glm::make_mat4(values)`。**删"等价"措辞**:现状 DX 代码(MeshFactory.cpp:692-704)是 latent bug——glTF matrix 列主序 `values[4c+r]=M[r][c]`,故 `XMFLOAT4X4=M^T`,转置得 M,但 DX 当行向量矩阵用,`XMVector3TransformCoord` 算 `p_row*M` = 列向量语境 `M^T*p_col`,而 glTF 要求 `M*p_col`,差一次转置(平移矩阵数值验证位置错乱)。`glm::make_mat4(values)` 直读列主序得正确 M,GLM 里 `M*v` 是正确的 glTF 变换。**make_mat4 是对 DX 现状 latent bug 的修正,不是等价替换**。迁移前先确认 character.glb 走 `ParseNodeLocalMatrix` 的 matrix 分支(710 行)还是 TRS 分支;若用 node.matrix 则姿态会变(**变对**)。TRS:`glm::translate(T)*glm::mat4_cast(glm::quat(rot.w,rot.x,rot.y,rot.z))*glm::scale(S)`(**glm::quat 构造 (w,x,y,z),glTF rotation (x,y,z,w),须重排**)。ComputeNodeWorld `local*parent`→`parent*local`。TransformCoord→`glm::vec3(M*glm::vec4(v,1))`;TransformNormal→`glm::vec3(glm::mat3(M)*v)`;cross→glm::cross。**CPU 预烘焙法线(953 行)**:`glm::transpose(glm::inverse(glm::mat3(world)))`(§3.4.1) |
| `src/Renderer/BurnFragmentSystem.{h,cpp}` | Render 入参 glm::mat4/vec3;删匿名 ns 手写数学(**封装 `normalizeOr(v, fallback)` 保留零向量回退**);**四元数映射**:`XMQuaternionRotationAxis(axis,angle)`→`glm::angleAxis(angle,axis)`(**参数序互换!**)、`XMQuaternionMultiply`→`o*delta`;**`XMVector3Rotate(vec,q)`(587 行)→ `q * vec`**(原清单漏列,补);上传 793 **无条件统一**(§3.4.2);**`m_sourceWorld`/`m_sourceNormalMatrix`(702-705,XMFLOAT4X4 缓存字段)及其 XMLoadFloat4x4 重读路径随同迁移到 glm::mat4**;**IncinerationField.h 调用点**:`BurnNoise`/`NormalizedDirection`/`Evaluate`/`EvaluateFromNoise` 4 个 XMFLOAT3 入参函数(非纯标量,改动量被原计划低估)随同改 glm::vec3 |
| `src/Renderer/DissolveParticleSystem.{h,cpp}` | 同 Burn;上传 608 **无条件统一**;**314 行 `XMVector3TransformNormal(normal, world)` 直接拿 world 变换法线(非 inverse-transpose)是 pre-existing bug**,通用映射 `TransformNormal→glm::vec3(glm::mat3(M)*v)` 会 **bug-for-bug 保留**(正确,但代码加注释"这里故意保留 bug,留待后续修") |
| `src/Renderer/IncinerationField.h` | 4 个 XMFLOAT3 入参函数(BurnNoise/NormalizedDirection/Evaluate/EvaluateFromNoise)→ glm::vec3 |
| `src/Renderer/RendererApp.{h,cpp}` | CameraViewProjection 返 glm::mat4;**SetMat4 重写 `glUniformMatrix4fv(loc,1,GL_FALSE,glm::value_ptr(m))`(加注释固化新约定)**;**法线矩阵 GPU 上传(909 行)用 `glm::transpose(glm::inverse(world))`,保持 mat4 对齐 uniform mat4**(§3.4.1);RenderSceneObjects 改 glm;XMFLOAT3/4 局部→glm |
| `src/Editor/EditorPanel.{h,cpp}` | `EditorPanelContext` 的 `XMFLOAT4&`→`glm::vec4&`;`&baseColor.x` 给 ImGui 仍有效(glm POD);`XM_2PI`→`glm::two_pi` |
| `src/Renderer/RenderDevice.cpp` | 无 DirectXMath;保持 `glDepthFunc(GL_LEQUAL)` 不动 |
| **新增** `src/Renderer/ShaderUniform.h` | 抽取统一 `inline void UploadMat4(GLuint,const char*,const glm::mat4&)`,消除三份重复上传 |
| 8 个 `assets/shaders/*.vert/.frag` | **零改动** |

**验证(整簇编译通过后,逐场景对拍)**:
- 静态网格 Standard 光照;
- glTF 节点变换:**Log 输出 world 16 float,与 Phase 2 黄金值比对**(**重点 rotation 四元数重排 + node.matrix latent bug 修正**);
- Dissolve 粒子 / Burn 碎片:**用 §3.4.2 客观校验**(临时走 SetMat4 约定对比);
- 法线光照;ImGui 拖控件;F5 热重载(**不要引入 uniform location 缓存**)。

**依赖**:Phase 3(稳定平台地基)+ Phase 2 黄金值锚点。

#### 3.4.4 Phase 4 回滚路径(原计划遗漏,补)

- Phase 4 在独立分支 `refactor/glm` 进行,整簇一个 commit;
- 若整簇编译失败或对拍严重偏离:该 commit 不 merge,分支整体丢弃;main 上 Phase 1/2/3 的成果不受影响(Phase 4 是纯数学层,无文件交集回滚阻力);
- 若仅个别函数语义错(如某 quaternion 序),在分支内 `git commit --amend` 修整,不污染 main;
- 黄金值文件(`docs/baselines/phase2-*.txt`)作为回归判定硬标准:偏离即回到上一版函数重查,不靠肉眼。

---

## 4. 风险与缓解(摘要)

- **数学主序陷阱**:transpose 参数(GL_FALSE+value_ptr 统一,SetMat4 加注释)、LH/RH(强制 `perspectiveLH_ZO`+`lookAtLH`,review 时 grep 禁裸 `glm::perspective(`/`glm::lookAt(`;`up`/`forward` 符号不翻)、**双转置怪癖**(§3.4.2 现状已证实为 bug,无条件统一,不保留)、**法线矩阵 mat3/mat4 路径区分**(GPU 上传 mat4、CPU 预烘焙 mat3,§3.4.1)、四元数参数序(`angleAxis` 角在前;`quat (w,x,y,z)` 重排;`XMVector3Rotate`→`q*vec`)、`glm::normalize` 无回退(封装 `normalizeOr`)、GLM_FORCE 对齐误开(禁止)。
- **glTF node.matrix latent bug**:`make_mat4` 是对 DX 现状 bug 的修正,迁移前确认 character.glb 走 matrix 还是 TRS 分支(§3.4.3)。
- **GLFW 上下文/profile**:core 下 `glEnable(GL_TEXTURE_2D)` 删除(3b);glad 生成版本=上下文版本;回退改 COMPAT;**glad 生成须含 imgui 用到的全部 enum**(§3.3.1)。
- **GLFW src 清单**:枚举式 12 个,排除 cocoa_*.mm 等(M4);**禁止给 GLFW `.c` 单独设 RuntimeLibrary**(S6)。
- **tinygltf ODR**:`TINYGLTF_NO_STB_IMAGE`/`_WRITE`/`USE_CPP14` 全局双配置加;`TINYGLTF_IMPLEMENTATION` 仅 cpp 内 define(§2.3)。
- **_CRT_SECURE_NO_WARNINGS**:第三方 C 源 per-item 加(§2.3/§2.4)。
- **删 WIC 后残留**:三处全迁 + `CoInitializeEx`/`CoUninitialize` 清理后才删 `windowscodecs.lib`;核对 `ole32.lib`(§2.5)。
- **ImGui 事件链**:回调注册顺序(create→InitForOpenGL→业务回调);鼠标 capture 语义(GLFW_CURSOR_DISABLED 模拟拖出窗口);REPEAT 连发(只 PRESS 触发 OnKeyPressed);**InputState 数组越界(扩 `m_down[512]`)**;DPI(SetProcessDpiAwarenessContext);窗口标题 UTF-8(WideToUtf8)。
- **F5 热重载**:保留 ReloadShaders 事务逻辑;不引入 uniform location 缓存。
- **命名空间**:glad 宏 `#define glXxx glad_glXxx`;include 顺序 glad 在前;`gladLoadGLLoader` 返回值断言(§3.3.1/§3.3.6)。
- **构建/链接**:GLFW 源码内编规避 LNK2038;glad.c + GLFW `.c` 走 C11(`LanguageStandard_C=stdc11`,不走 `/TP`);第三方 C 源 per-item 降 WarningLevel + `_CRT_SECURE_NO_WARNINGS`。

---

## 5. 回滚策略

- 每阶段一个分支:`refactor/stb`、`refactor/tinygltf`、`refactor/glfw-glad`(内含 3a/3b/3c 子 commit)、`refactor/glm`(整簇单 commit);
- 验证通过才 merge 回 main;
- **Phase 1/2/3 三者互相独立**(均仅依赖 Phase 0),互不影响、可并行;Phase 3 阻塞 Phase 4;Phase 4 依赖 Phase 2 黄金值;
- **Phase 4 回滚路径**(§3.4.4):整簇 commit 不达标即分支丢弃,不污染 main;
- `OpenGL.h/cpp` 与 DirectXMath 代码在 git 历史,可恢复。

---

## 6. 工作量估算

| 阶段 | 改动文件数 | 复杂度 |
|---|---|---|
| Phase 0 Vendoring | 0 代码 + ~30 external + 1 vcxproj | 低 |
| Phase 1 stb(含 COM 清理) | ~7 | 低-中,半天 |
| Phase 2 tinygltf(含黄金值落盘) | ~5 | 中-高,1-2 天 |
| Phase 3a vendoring 编译 | ~1 vcxproj + glfw_config.h | 低,半天 |
| Phase 3b 窗口/事件/ImGui 平台后端 | ~9 | **高**,1.5-2 天 |
| Phase 3c loader 替换 + 删 OpenGL.h | ~7 | 中,1 天 |
| Phase 4 GLM + shader 统一(原子簇) | ~14 cpp/h + 0 shader | **最高(风险)**,2-3 天 |

**总计**:约 35-40 个文件改动点,Phase 1/2/3 可并行准备,Phase 3 内部串行 3a→3b→3c,Phase 4 最后,全程 7-10 个工作日。

---

## 7. 关键文件路径(实施时定位)

- `YRender.vcxproj`:38 IncludePath(无 Condition,改一次两配置生效)、44/59 Preprocessor(Debug/Release)、52/68 链接库、72-94 ClCompile、95-118 ClInclude、35-39 PropertyGroup。
- `src/Renderer/OpenGL.{h,cpp}`(Phase 3c 删除;5 个消费者:RendererApp/RenderDevice/BurnFragmentSystem/DissolveParticleSystem/ResourceManager)。
- `src/Renderer/RenderDevice.cpp`:原 wglCreateContext(41)/LoadOpenGLFunctions(46)/wglSwapIntervalEXT(50-52)/`glEnable(GL_TEXTURE_2D)`(58)/CaptureBackBufferPng(141-209)。
- `src/Renderer/RendererApp.cpp`:法线矩阵 GPU 上传(909,mat4)/uNormalMatrix(911)/SetMat4(1299-1300)/ReloadShaders(579-635)。
- `src/Renderer/BurnFragmentSystem.cpp`:m_sourceWorld/m_sourceNormalMatrix(702-705)/上传(793)/`XMVector3Rotate`(587)。
- `src/Renderer/DissolveParticleSystem.cpp`:上传(608)/`XMVector3TransformNormal` pre-existing bug(314)。
- `src/Scene/Camera.cpp`:view*projection(82-84)/`XMConvertToRadians(45.0f)`(83)。
- `src/Scene/SceneTypes.h`(根类型,Phase 4 整簇核心)。
- `src/Assets/MeshFactory.cpp`:node.matrix latent bug(692-704)/ParseNodeLocalMatrix matrix 分支(710)/CPU 预烘焙法线(953)/position 预烘焙世界(966/978)。
- `src/Assets/ResourceManager.{h,cpp}`(Phase 1 WIC 路径)。
- `src/YRender.cpp`:`CoInitializeEx`(8)/`CoUninitialize`(11)。
- `external/imgui/backends/imgui_impl_opengl3.cpp`(`YRender::gl` 命名空间耦合点,走路线 a);`imgui_impl_glfw.{cpp,h}`(已 vendored);`imgui_impl_opengl3_loader.h`(留树不入构建)。
- `docs/baselines/phase2-*.txt`(Phase 2 黄金值,Phase 4 锚点)。
- `build.ps1` / `scripts/verify.ps1`。
