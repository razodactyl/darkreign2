# DirectX Research Notes

## Current Renderer Version

Dark Reign 2 is currently using a **DirectX 7-era renderer**.

Evidence in the codebase:

- `3rdparty/dx7/` contains the bundled SDK headers in active use.
- `graphics/vid_decl.h` typedefs the renderer interfaces to:
  - `LPDIRECTDRAW7`
  - `LPDIRECT3D7`
  - `LPDIRECT3DDEVICE7`
- `graphics/vid.cpp` queries `IID_IDirect3D7` and uses DirectDraw cooperative-level fullscreen/windowed switching.

So, if choosing between DX7, DX8, and DX9 as the current class, the answer is **DX7**.

## dgVoodoo

dgVoodoo translates DX7 onto a modern backend at runtime. It ships with the
game but is **invisible to the build**: there are no references to it in any
source file or project file. It is a drop-in replacement `DDraw.dll` /
`D3DImm.dll` placed beside the executable.

The shipped copy and its config are tracked under
`resources/DR2 Online/DR2 Total Patch 145800/`:

```
DDraw.dll  D3DImm.dll  dgVoodoo.conf  dgVoodooCpl.exe
```

Settings worth knowing when debugging display behaviour (`dgVoodoo.conf`):

| Setting | Value | Effect |
| --- | --- | --- |
| `OutputAPI` | `bestavailable` | picks the D3D11/12 backend |
| `AppControlledScreenMode` | `true` | dgVoodoo does not override our mode choice |
| `Resolution` | `unforced` | no upscale forced at the wrapper layer |
| `VideoCard` / `VRAM` | `geforce_fx_5700_ultra` / `256` | what our caps enumeration actually sees |
| `DisableAltEnterToToggleScreenMode` | `true` | Alt+Enter is ours to handle |

Because `Resolution` is unforced and the screen mode is app-controlled, the
resolution and windowing decisions made in `graphics/vid.cpp` are the ones that
take effect — dgVoodoo passes them through rather than substituting its own.
That is what makes `-borderless` and the DPI work below meaningful rather than
being overridden by the wrapper.

Note the implication for a dev environment: a build run from the repo does not
automatically get these DLLs, so it talks to whatever `ddraw.dll` the OS
provides unless they are copied alongside the binary.

## `-borderless` in This Codebase

`-borderless` is implemented as a **windowed DirectDraw mode** with borderless
window styling, not as exclusive fullscreen.

That is the safer approach for this renderer because the original fullscreen
path depends on:

- exclusive DirectDraw cooperative level
- display mode switching via `SetDisplayMode`
- fullscreen swap behavior tied to DX7-era assumptions

Using a popup window avoids forcing a display mode change and generally gives
more stable modern Windows behavior.

### Usage

```
dr2.exe -borderless             # covers the whole desktop
dr2.exe -borderless:640x480     # explicit size, centred
dr2.exe -borderless=1920x1080   # ':' and '=' are both accepted
```

The value is attached to the switch with `:` or `=`. A space-separated form
(`-borderless 640x480`) is **not** supported: `NextArg` in `main/maininit.cpp`
treats whitespace as the argument terminator, and changing that would affect
every other switch in the game.

`-vidmode:WxH` still exists and shares the same parser. It selects a fullscreen
display mode; `-borderless:WxH` sizes the borderless window. Both write
`Main::vidModeX/Y`, so there is no reason to pass both.

### Implementation notes

- `main/maininit.cpp` — `ParseVidMode()` is the shared `WxH` / `max` parser used
  by both `-borderless` and `-vidmode`.
- `graphics/vid.cpp`, `Vid::Init()` — when borderless, `viewRect` is seeded from
  `Main::vidModeX/Y` if a size was given, otherwise from
  `GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)`. `InitDD()` has not run at that
  point, so the display size cannot come from `CurMode()` yet.
- `Vid::SetMode()` clamps the requested size to the desktop rect and to
  `MINWINWIDTH`/`MINWINHEIGHT` (640x480). The old `MAXWINWIDTH = 1900` ceiling
  is already commented out in `graphics/vid_private.h`, so 4K is not blocked.

Before this change, `-borderless` always produced a 640x480 popup: the size came
from `STARTWIDTH`/`STARTHEIGHT`, `-vidmode` only fed the *fullscreen* mode list
via `PickVidMode()`, and `vid.cpp` deliberately skips restoring
`Settings::viewRect` when borderless.

## DPI Awareness

The process previously declared no DPI awareness at all — no manifest, no
`SetProcessDpiAwareness` call, nothing in `appdr2.vcxproj`. On a 4K panel at
150% or 200% scaling that meant the process was DPI-unaware, so:

- every screen metric read back was virtualised rather than true pixels
- the desktop compositor bitmap-scaled the window, producing a blurry image
- "borderless covering the screen" was computed in virtualised coordinates

`Main::SetupDpiAwareness()` in `main/maininit.cpp` now opts in, resolving entry
points at runtime and degrading gracefully on older systems:

1. `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` — Windows 10 1703+
2. `SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE)` — Windows 8.1+
3. `SetProcessDPIAware()` — Windows Vista+

It runs from `Main::Init()`, after `LowLevelSystemInit()` so the result can be
logged, and well before `CoreSystemInit()` creates the main window. Which tier
was selected appears in the log as `DPI awareness: ...`.

Note the visible consequence on a scaled display: the game now renders at true
pixel resolution instead of being upscaled, so at 200% scaling a fixed-size
window is physically half the size it used to appear. That is the intent for 4K
work, but it is a behaviour change.

## Size of the DX7 Dependency

The renderer surface is smaller and better abstracted than it looks. Only **13
of the 163 files** in `graphics/` touch DirectDraw or Direct3D types at all:

```
bitmap.cpp  bitmap.h  bucket.cpp  light.h  Vertex.h  vid.cpp  vid_cmd.cpp
vid_cmd_dialog.cpp  vid_decl.h  vid_enumdx.cpp  vid_public.h  viderror.cpp
vidrend.cpp
```

Of roughly 220 raw `device->` calls, **171 are in `vidrend.cpp`** alone. Terrain,
mesh, particle and effect code all goes through `Vid::SetTexture`,
`Vid::DrawIndexedPrimitive` and friends. The abstraction boundary a port would
need already exists.

## Upgrade Target: DX9 (on the roadmap)

DX9 remains the intended same-family target. **DX8 should be skipped** — it is a
transition API with no lasting benefit.

### What ports near-mechanically

- **FVF vertex formats** (`graphics/Vertex.h`) — the `D3DFVF_*` constants are
  unchanged in DX9. The one exception is `D3DFVF_RESERVED1` in `FVF_LVERTEX`,
  which is a DX7 specular-reserved slot with no DX9 equivalent.
- **`DrawPrimitive` / `DrawIndexedPrimitive`** from user memory map onto the DX9
  `*UP` variants.
- **Texture stage states** — 136 `SetTextureStageState` calls in `vidrend.cpp`.
  Fixed-function stages survive into DX9 essentially intact.
- **Render states** — `D3DRENDERSTATE_*` becomes `D3DRS_*`; a handful are dropped.
- **Fixed-function T&L** — lights, materials and transforms are only about a
  dozen call sites across `light.cpp` and `vid_math.cpp`.

### What does not port — the actual work

- **All of DirectDraw.** DX9 has no DDraw. `Bitmap` is built directly on
  `LPDIRECTDRAWSURFACE7` (`bitmap.h`), created via `CreateSurface` with
  `DDSCAPS_TEXTURE` and then `QueryInterface`'d to `IID_IDirect3DTexture2`
  (`bitmap.cpp`). 19 `CreateSurface` sites in `vid.cpp`, 31 `Lock()` in
  `bitmap.cpp`.
- **Presentation.** `front->Flip()` and `front->Blt()` collapse into a single
  `Present()`. Cooperative levels plus `SetDisplayMode` become
  `D3DPRESENT_PARAMETERS`.
- **Colour key.** `SetColorKey(DDCKEY_SRCBLT)` in `bitmap.cpp` is gone in DX9;
  it has to become load-time alpha conversion.
- **Enumeration.** All 1252 lines of `vid_enumdx.cpp` — DDraw driver enumeration,
  D3D7 device enumeration and mode callbacks. DX9 replaces this with a much
  shorter `EnumAdapterModes` loop, but it is a rewrite, not a translation.
- **Caps.** `DriverD3D` in `vid_decl.h` mirrors `D3DDEVICEDESC7` into ~15
  bitflags. `D3DCAPS9` is shaped differently.
- **Lost device handling.** DDraw's per-surface "lost + `Restore()`" model in
  `Vid::RestoreSurfaces()` becomes `TestCooperativeLevel` plus a full `Reset()`
  with every `D3DPOOL_DEFAULT` resource released first. Different enough to need
  redesign rather than porting.

### Rough sizing

`bitmap.cpp` (3696) + `vid.cpp` (2344) + `vid_enumdx.cpp` (1252) ≈ **7,300 lines**
are the real port surface. `vidrend.cpp` (1622) is mostly find-and-replace. The
other ~150 files in `graphics/` should not need to change if the `Vid::` API
keeps its signatures.

### Honest cost/benefit

dgVoodoo already translates DX7 to D3D11/12, and it wraps DX9 too — so a DX9
port would either sit under dgVoodoo again or replace it. It fixes nothing that
dgVoodoo already fixes.

What DX9 does buy:

- no external drop-in DLL dependency
- native control over presentation, windowing and 4K
- honest `Reset()` / device-lost semantics
- a programmable pipeline available later if wanted

## Roadmap

**Done — cheap wins on the existing DX7 backend:**

- DPI awareness
- `-borderless` defaulting to desktop resolution
- `-borderless:WxH` carrying the mode on a single switch

**Next, if a port is pursued:** go to DX9, strictly behind the existing `Vid::`
API so the rest of the engine does not notice. Treat the `bitmap.cpp` surface
layer as the first and hardest step; presentation and enumeration follow from it.

**Longer term:** D3D11 direct is defensible given the same abstraction boundary,
but it means hand-writing fixed-function emulation for those 136 texture-stage
calls. Only worth it if shader-era features are actually on the roadmap. See also
the OpenGL work on the `upgrade-graphics-system` branch.

## Summary

- **Current renderer class:** DX7, via dgVoodoo at runtime (`AppControlledScreenMode`,
  so our own mode logic is authoritative)
- **DX7 surface area:** 13 files, ~7,300 lines of real port work
- **Best same-family modernization target:** DX9
- **DX8 recommendation:** skip it
- **Risk level of upgrade:** significant renderer port, not a minor change
