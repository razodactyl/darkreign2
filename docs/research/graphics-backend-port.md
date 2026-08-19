# Graphics Backend Port: Research Notes

Status: **the game renders through OpenGL**. Implementation is tracked on
`vid-merge-ogl-backend`; see [Progress](#progress) at the end. `-ogl` plays -
menus, missions and the online lobby - with no DirectDraw anywhere. What is left
is fidelity and performance, not bring-up.

This note records what was learned from the abandoned
`origin/upgrade-graphics-system` branch (2020), why it stalled, and which of its
decisions we are deliberately *not* repeating.

## Background

The original 2020 effort had two goals stacked on top of each other:

1. Replace the DirectX 7 renderer (see [DirectX Research Notes](../../RESEARCH.md))
   with something modern, and
2. Replace the native Win32 window and message pump with GLFW, as a first step
   toward a multiplatform build (Linux / macOS).

The long-term intent at the time was **Vulkan**, with OpenGL as an intermediate
stepping stone. That intent is ~6 years old as of 2026 and should be re-evaluated
before anyone acts on it — see [Multiplatform, revisited](#multiplatform-revisited).

## What the branch contains

The diff against its fork point (`e2356338`) reads as 299 files and ~74k added
lines, but that is almost entirely noise:

| Category | Rough size |
| --- | --- |
| Vendored `glfw-3.3.2.bin.WIN32`, including its full HTML doc tree | ~68k lines |
| `.vcxproj` include-path churn across every project in the solution | ~1.5k lines |
| **Actual renderer work** | **~5 files** |

The real work lives in:

- `graphics/vidrend.cpp` — 138 GL calls; the draw path
- `graphics/vid.cpp` — 17 GL calls; context and frame lifecycle
- `graphics/bitmap.cpp` / `bitmap.h` — texture upload, adds a `glTextureId` member
- `main/maininit.cpp` — 9 GL calls; GLFW window creation
- `README.md` — the GLSL uber-shader, pasted in as a fenced code block

Notably, the branch **already models the runtime-flag architecture** we want.
There is a `Vid::isStatus.ogl` bit, and `DrawPrimitive` / `DrawIndexedPrimitive`
already read `if (!Vid::isStatus.ogl) { ...D3D... }` and then fall through to a
GL block. The shape is right; the execution is inline and duplicated.

## Why it stalled

The tip commit is titled *"Partially working. Input has issues."* The root cause
is windowing, not rendering.

`maininit.cpp` on that branch creates the normal Win32 window, then calls
`glfwCreateWindow` to make a **second** window, then does:

```cpp
ShowWindow(mainHwnd, SW_SHOWMINIMIZED);
```

So the game runs with two windows and two input paths, with DirectInput bound to
the minimized one. The knock-on effects went well past the cursor:

- Replacing the native HWND pump disturbed **game processing and multiplayer**,
  because the message loop is load-bearing for far more than input.
- Fullscreen / borderless / mode-switch logic is written against DirectDraw
  cooperative levels and the real HWND, and had no GLFW equivalent.

**Conclusion: the windowing rewrite and the renderer rewrite must not be the same
project.** Coupling them is what killed the 2020 attempt.

## Other issues in the branch, for the record

1. **It cannot build.** The `.vcxproj` files reference
   `../3rdparty/glew-2.1.0/include` and `main/main.h` does
   `#pragma comment(lib, "glew32s.lib")`, but GLEW was never committed — only
   GLFW is in the tree.
2. **Only `FVF_TLVERTEX` is implemented.** Every other vertex type hits an early
   `return D3D_OK`, silently drawing nothing. The terrain and mesh paths are stubs.
3. **Per-draw uniform lookups.** ~20 `glGetUniformLocation` calls plus a fresh
   `glBufferData` on every primitive. Correct-first is defensible; it is a known
   performance cliff to revisit.
4. **Shaders are not assets.** They live only in the README, and are loaded at
   runtime from `<install dir>/library/shader/*.glsl`.
5. **`glfwredef.h`** exists solely to paper over a calling-convention mismatch by
   `#define`-ing every GLFW entry point to `_cdecl` and then `#undef`-ing it.
   A symptom of the DLL/static-lib confusion above.

## What master did in the meantime

Master has diverged in exactly the areas the branch touched. Since the fork point
it gained `interface/pixelscale.cpp` (~917 lines of 4K UI scaling) and substantial
rework of `font.cpp`, `icontrol.cpp`, and `iface.cpp`.

The branch's `interface/iface_util.cpp` and `interface/font.cpp` changes are
therefore **unsalvageable** — that work has to be redone against master's 4K
scaling system rather than merged.

## The seam

The one genuinely encouraging finding: DirectX is well contained. Outside
`graphics/`, the entire codebase has exactly one direct D3D reference —
`graphics/vid_public.h:453`, inside `RenderClear`.

Everything else already funnels through a small, stable API:

- Frame: `Vid::RenderBegin` / `RenderEnd` / `RenderFlush` / `RenderClear`
- Geometry: `Vid::DrawPrimitive` / `DrawIndexedPrimitive`
- State: `SetRenderState`, `SetTextureState`, src/dst blend, cull, material,
  texture factor, fog, z-bias
- Textures: `Bitmap::LoadVideo` / `ReleaseDD`, `Bitmap::Manager::SetTexture`
- Transforms: `graphics/vid_math.cpp`
- Enumeration: `graphics/vid_enumdx.cpp`

Files with direct DirectDraw/D3D contact, for reference:

```
graphics/bitmap.cpp          graphics/vid.cpp             graphics/vid_math.cpp
graphics/bitmap.h            graphics/vid_cmd_dialog.cpp  graphics/vid_public.h
graphics/bitmap_manager.cpp  graphics/vid_decl.h          graphics/vidrend.cpp
graphics/light.cpp           graphics/vid_enumdx.cpp      interface/input.cpp
graphics/light.h
```

(`interface/input.cpp` is DirectInput, unrelated to the render backend.)

## Decisions taken

| Decision | Choice | Reason |
| --- | --- | --- |
| Windowing | **Keep the native Win32 HWND**; create the GL context on it via WGL | The 2020 stall was caused by replacing the HWND pump, which broke multiplayer and game processing. Renderer and windowing stay decoupled. |
| GL loader | **glad** over GLEW | Single generated `.c`/`.h`, no static lib to build or ship. Removes the missing-`glew32s.lib` problem outright. |
| GL version | **3.3 core** | The branch's shaders say `#version 400` but use nothing 4.x-specific. 3.3 covers essentially every GPU and driver from 2010 on, including the older Intel integrated parts likely to be running a 2000-era RTS. |
| Backend dispatch | Struct of function pointers, populated at init | No vtables; matches the codebase's namespace/C style. One indirect call on the hot path. |
| Sequencing | The D3D-only backend seam lands as its own change, before any GL code | Pure refactor, verifiable against the running game, keeps the GL diff reviewable. |
| GLFW | **Dropped** | Along with `glfwredef.h`, the vendored binaries, and the doc tree. |

## Multiplatform, revisited

The Linux/macOS port remains a goal, but is explicitly **out of scope** for the
OpenGL backend work. Deferring it is what makes the renderer work tractable.

When it is picked up, the 2020 assumptions should be re-examined rather than
resumed:

- **Vulkan as the target** was a 2020 judgement. Since then WebGPU implementations
  (`wgpu`, Dawn) have matured into credible portable abstractions, and macOS
  remains Metal-only with OpenGL deprecated since 10.14 — meaning a GL backend
  does not actually deliver macOS on its own.
- **GLFW vs SDL.** SDL2/3 covers input, audio, gamepads, and window management in
  one dependency, and is closer in scope to what this codebase actually needs
  from a platform layer. GLFW covers window + input only.
- The blocker is not the renderer. It is Win32, DirectInput, DirectSound/Miles,
  Bink, the WON/Styxnet networking layer, and the MSVC-specific build. The
  renderer is one item on that list, and — per the seam described above — one of
  the better-isolated ones.

The right sequencing is: land the backend seam, get a working alternate GL
pipeline behind a flag, and only then evaluate whether the platform layer moves
to SDL and whether the eventual third backend is Vulkan, WebGPU, or something else.

## See also

- [DirectX Research Notes](../../RESEARCH.md) — current DX7 renderer, dgVoodoo wrapper
- [Interface Research Notes](../../interface/RESEARCH.md) — 4K UI scaling system
- [Networking Research Notes](../../NETWORKING_RESEARCH.md)

## Progress

### Phase 0 - dependencies (done)

`3rdparty/glad/` holds a glad2-generated GL 3.3 core loader plus the WGL
extensions needed to create a core-profile context. Plain `.c`/`.h`, compiles
clean at `/W4`, no static library to build or ship. See its README for the exact
regeneration commands.

### Phase 1 - backend seam, D3D-only (done)

`graphics/vid_backend.h` declares `Vid::Backend`, a table of function pointers
covering the frame, render state, texture stages, transforms, materials, lights
and geometry. `Vid::backend` points at the table in use; `Vid::SelectBackend` is
called once from `Vid::Init`.

`graphics/vid_backend_d3d.cpp` is the DirectX 7 table. Its contents were moved
out of `vidrend.cpp`, `vid_cmd_dialog.cpp`, `vid_math.cpp` and `light.cpp`
unchanged - the same code, reached through a pointer.

The split is: **the public `Vid::SetXState` functions keep their `renderState`
bookkeeping and return values; only the lines that talked to the device moved.**
No caller outside `graphics/` changed.

After this, `device->` appears nowhere except `vid_backend_d3d.cpp` and the
enumeration code (`vid.cpp`, `vid_enumdx.cpp`, `bitmap_manager.cpp` reporting),
which is Phase 6 work.

Two things to know when the GL backend lands:

- `Vid::ClearFlags` still carries `D3DCLEAR_*` values, and `Backend::SetViewport`
  still takes a `ViewPortDescD3D`. Both are structurally neutral - the latter is
  only `{x, y, w, h, minZ, maxZ}` - so they carry over, but they should be
  renamed once a second backend exists.
- `Vid::SetTexture` and `SetTextureDX` still `return dxError == DD_OK`, reading
  the global set inside the backend. That works for D3D; a GL backend will need
  these to return a value rather than leave one in `dxError`.

One behavioural trap found and avoided during the move: `SetSpecularStateI`
guards its device call with `#ifdef DOSPECULAR` (which is *not* defined), while
`SetRenderState` sets `D3DRENDERSTATE_SPECULARENABLE` unguarded. Folding the
guard into the backend would have silently disabled a live call. The guard stays
at the `SetSpecularStateI` call site.

### Containment pass (done)

Phase 1 left DirectX values and types showing through the supposedly neutral
layer. That defeats the point of the seam, so it was cleaned up before going
further.

**`graphics/vertex.h`** is the vocabulary the whole game speaks - `PT_*`,
`FVF_*`, `DP_*`, `RS_SRC_*`, `RS_DST_*`, `RS_TEX*`. Every one of those was
literally defined as a Direct3D constant (`RS_SRC_SRCALPHA = D3DBLEND_SRCALPHA
<< RS_SRC_SHIFT`, and so on), so the game-wide blend vocabulary *was* the D3D
enum. They are now self-contained, with new `BLEND_FACTOR`, `TEXTURE_ADDRESS`
and `VERTEX_FORMAT` enums behind them.

The numbers are unchanged, so the D3D backend still passes them through
untranslated. That is now an explicit, checked optimisation rather than a silent
assumption: `vid_backend_d3d.cpp` carries 35 `static_assert`s tying each constant
to the D3D one it was derived from. If they ever diverge the build breaks instead
of the renderer.

**`graphics/vid_public.h`** no longer publishes `dxError`, `LOG_DXERR`, or the
`front` / `ddx` / `d3d` / `device` handles. Those moved to a new
`graphics/vid_dx.h`, included only by the six files that genuinely talk to
DirectX. `GetErrorString` stayed public - `interface/input.cpp` uses it to report
*DirectInput* errors, so it is an HRESULT helper rather than a device handle.

`Vid::ClearFlags` no longer carries `D3DCLEAR_*` values, and `Vid::ViewPort`
replaced `ViewPortDescD3D` in the backend interface and in the `viewDesc` global.

The `*D3D` / `*DX` suffixes on the public API were also misleading, since none of
those functions touch DirectX any more - they go through the backend table like
everything else:

| Was | Now |
| --- | --- |
| `SetCullStateD3D` | `SetCullState` |
| `SetFogColorD3D` | `SetFogColorI` |
| `SetMaterialDX` | `SetMaterialI` |
| `SetTextureDX` | `SetTextureI` |
| `SetWorldTransform_D3D` | `SetWorldTransformI` |
| `SetViewTransform_D3D` | `SetViewTransformI` |
| `SetProjTransform_D3D` | `SetProjTransformI` |

`Vid::SetTexture` and `SetTextureI` also stopped reading `dxError` to decide
their return value - `Backend::BindTexture` and `DisableTexStage` report success
directly.

After this, `vertex.h` and `vid_backend.h` contain no DirectX at all, and the
only D3D names left in `vid_public.h` are `InitDD` / `InitD3D` / `ReleaseDD` /
`ReleaseD3D`, which really are DirectX device-creation calls and belong to
phase 6.

### Specular (enabled)

`DOSPECULAR` in `graphics/bitmap.h` had been commented out since the DX6-to-DX7
era, and the code behind it had rotted: both `lightvertscamera.cpp` and
`lightvertsmodel.cpp` tested `light->d3d.dwFlags & D3DLIGHT_NO_SPECULAR`, but
`dwFlags` was a `D3DLIGHT2` member that `D3DLIGHT7` does not have, and `d3d` is
private. D3D7 has no per-light specular disable, so the test is simply gone and
the global setting is all that is consulted.

There is a trap here worth knowing about. DR2 draws pre-transformed vertices, and
for those D3D takes the **fog factor from the specular alpha channel** - which is
why `SetRenderState` has always forced `D3DRENDERSTATE_SPECULARENABLE` on with a
bare "dx fog" comment. Letting the now-live `vid.specular` toggle drive that
render state would have turned fog off along with specular. So the device state
stays on permanently, and `renderState.status.specular` gates only the specular
*lighting* maths. Both sites are commented to that effect.

### Phase 2 - OpenGL context (done)

`graphics/vid_backend_ogl.cpp` holds the GL 3.3 core backend, selected with
`-ogl` (`Vid::doStatus.ogl`, wired in `main/maininit.cpp` beside `-borderless`).

Context creation is the standard two-step: a throwaway window and legacy context
load the WGL extensions, then `wglCreateContextAttribsARB` makes a 3.3 core
context **on the game's existing `HWND`**. The window, the message pump and
DirectInput are untouched - that is the whole point, given what replacing them
cost in 2020. If any of it fails, `Vid::Init` logs and falls back to DirectX 7
rather than running a backend whose every call is a no-op.

Implemented so far: context create/destroy, `SwapBuffers`, `glClear`, and
`glViewport` (with the y-flip, since GL's origin is bottom-left). Everything that
draws is a silent stub annotated with the phase that will fill it in.

**`-ogl` is not a playable mode yet.** `Vid::Init` still runs the whole
DirectDraw/Direct3D setup after bringing up the context, and no geometry reaches
the screen. It logs a warning saying so. It exists so the context can be proven
against the real window and input before any rendering depends on it.

Also fixed while adding the `ogl` status bit: `Vid::Status::ClearData` called
`Utils::Memset(this, 0, sizeof(this))` - the size of a *pointer*. It happened to
clear every bit while the bitfield fitted in 4 bytes, and would have started
silently leaving members uninitialised the moment it grew past 32 bits.

### Phase 3 - textures (done)

The useful discovery here is that the engine already knows how to work with a
bitmap that has no DirectDraw surface - that is what `bitmapNORMAL` is, and
`Lock`, `UnLock`, `Clear`, `SetSurfaceColorKey` and `CreateMipMaps` all check
`surface` and take a system-memory path when it is null.

So a texture under OpenGL is just **a system-memory bitmap plus a texture
object**. `Bitmap::Create` takes that branch when `-ogl` is set: it owns the
pixels, and `Bitmap::backendTex` holds an opaque handle from the backend.
Everything that manipulates pixels - the BMP/TGA/PIC readers, `CopyBits`, font
glyph rendering, `pixelscale.cpp`, Bink - is untouched and keeps working.

Three ops were added to the backend table:

| Op | DirectX 7 | OpenGL |
| --- | --- | --- |
| `TextureCreate` | returns 0; it samples from its own DirectDraw surface | `glGenTextures` |
| `TextureDestroy` | nothing | `glDeleteTextures` |
| `TextureUpload` | nothing | `glTexImage2D` + `glGenerateMipmap` |

`BindTexture`, `SetTexWrap` and `SetTexFilter` are now real on the GL side too;
only `SetTexBlend` stays stubbed, because how the stages combine is a decision
for the shader in phase 5.

**Pixel format.** There is no texture format enumeration to run under GL, so
`Vid::InitFormatsOGL` registers exactly one: A8R8G8B8. That is what the bitmap
code already expects of a 32-bit surface, and in memory it is precisely
`GL_BGRA` + `GL_UNSIGNED_INT_8_8_8_8_REV`, so uploads need no conversion. All of
`PixNormal` / `PixTranslucent` / `PixTransparent` point at it.

**Staying fresh.** `Lock`/`UnLock` is the engine's universal "I am about to write
pixels / I am done" idiom, so `UnLock` is where the backend texture is refreshed.
That covers writable and animating textures, including Bink video, without
special-casing any of them. A `texDirty` status bit keeps it to one upload per
change.

### Phase 3b - running without DirectDraw (done)

`-ogl` now brings the whole engine up on its own: **Intro -> Shell -> Mission ->
Load -> SimInit -> Sim**, with no DirectDraw or Direct3D device anywhere. Nothing
is drawn yet, but every other system runs.

**DirectDraw had to go, not just be ignored.** The 2020 branch's `SetModeGL`
still called `InitDD`, `SetCoopLevel` *and* `InitD3D` - it kept the entire
DirectDraw/Direct3D device alive and ran GL beside it. That worked for them only
because GLFW gave them a second window: DirectDraw owned the original HWND and GL
owned the GLFW one. The two-window hack that broke their input is also what let
them dodge this conflict.

With a single window the two fight over it, measured on a Parallels/M1 VM with
dgVoodoo:

| | |
| --- | --- |
| `DirectDrawEnumerateEx` with a GL context on the same window | ~11,000 ms, and frequently never returning |
| `InitOGLDrivers` + `InitOGLDevice` | 239 ms |

So the bypass is forced by the one-window decision, not a preference.
`InitOGLDrivers` stands in for `InitDD` with one synthetic driver and one mode
(the desktop); `InitOGLDevice` stands in for `InitD3D` + `InitSurfaces`;
`RenderFlush` presents with `SwapBuffers`; and `ClearBack` goes through the
backend, since `backBmp` is only a description of the screen on this path.

**Two traps, both found by running it rather than reasoning about it:**

1. `ASSERT(pixForm)`. The GL texture format was registered early in `Vid::Init`,
   but `InitDD` calls `ReleaseDX`, which calls `ReleaseD3D`, which does
   `pixFormatList.DisposeAll()`. Anything registered before that point is
   silently thrown away. Registration has to sit inside `InitOGLDevice`, exactly
   where `InitD3D` registers its own.

2. An access violation reading `0x000000C8`. `InitD3D` is not just device
   creation - roughly half of it is backend-neutral setup, including
   `mainCamera = curCamera = new Camera("main")`. Skipping it left `curCamera`
   null, and `Vid::CurCamera()` dereferences it on every render path.
   `InitOGLDevice` now does that half too: the camera, the `OnModeChange`
   callbacks across Bitmap/Material/Light/Mesh/Command/Options/Graphics,
   `InitResources`, `SetGamma`, and the initial transforms.

**Diagnosing crashes.** appdr2 Debug emits a linker map file now. Note the
built-in stack walker follows EBP chains and produces mostly garbage frames in
this build - resolving the *faulting* address against the map is useful, the rest
of the stack is not. Breadcrumb logging was what actually localised both bugs.

Both paths were verified by running the game: `-ogl` reaches `Sim` clean, and the
DirectX 7 path still creates real surfaces and a device, plays a mission, and
renders correctly on screen.

### Phase 4 - rendering (done), and why phase 5 vanished

The plan split this into a 2D/interface pass and a separate 3D pass. That split
does not survive contact with the code.

DR2 builds with `DODXLEANANDGRUMPY` - "do only TLVERTS". The engine software-
transforms **everything**, terrain and meshes included, into pre-transformed
`FVF_TLVERTEX` before it reaches the device. Instrumenting the draw path
confirmed it: ~95 draw calls a frame, **0 rejected**, no other vertex format used
at all. So implementing that one format renders the whole game. Phases 4 and 5
collapsed into one.

The backend compiles a single program at context creation, with a VAO over the
`VertexTL` layout - guarded by `static_assert`s on the offsets, because the
vertices come straight off bucket memory. `Color` is `b,g,r,a` in memory, so
`GL_BGRA` as the attribute size does the reordering rather than a shader swizzle.

**Perspective correction.** The vertices are already projected and carry `rhw`
(1/w). Emitting them with `w = 1` puts them in the right place but makes GL
interpolate every varying in screen space - affine mapping, which makes textures
visibly zig-zag across perspective surfaces. This is the artefact the 2020 branch
shipped with and never fixed. Restoring `w` and pre-multiplying x/y/z by it
yields the same screen position after the perspective divide while giving GL the
`w` it needs. 2D geometry sets `rhw = 1` and comes through unchanged.

**Fog.** No range needed: the software transform has already baked the
per-vertex fog factor into the specular alpha channel - which is exactly why the
device's specular state has to stay on, as noted when specular was enabled. The
shader only needs the on/off flag and the colour.

**Two bugs that only playing it would have found:**

1. The texture-swap "turtle" drew every frame in the top right, because
   `FreeVidMem` has no DirectDraw to ask and `totalTexMemory` is therefore 0.
2. `Bitmap::Create` can run twice on the same bitmap. On the D3D path the pixels
   live in the surface so nothing is lost; here they are ours, and the old buffer
   leaked - two 256x256x4 textures, 512KB, visible in the shutdown leak report.

### Next

Not bring-up any more - fidelity and performance:

- **`SetTexBlend` is still a no-op**, so every texture stage combines as
  MODULATE. The `RS_TEX_*` ops (decal, add, modulate2x/4x) and the second texture
  stage are the main remaining visual gap.
- **Per-draw `glBufferData` orphaning** on every one of ~95 draws a frame. A
  persistent ring buffer is the obvious win.
- **Uniforms are pushed per draw** rather than when they change.
- `caps.texNoHalf` is set TRUE, so the half-texel shift is skipped. Worth a
  careful look at 2D text and icon alignment.
- Fullscreen and mode switching: `InitOGLDrivers` reports one mode, the desktop.
- The options dialog and `vid_settings` still describe DirectDraw drivers.
