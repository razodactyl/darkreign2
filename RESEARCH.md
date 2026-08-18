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

## What `--borderless` Means in This Codebase

The new `--borderless` switch is implemented as a **windowed DirectDraw mode** with borderless window styling, not as exclusive fullscreen.

That is the safer approach for this renderer because the original fullscreen path depends on:

- exclusive DirectDraw cooperative level
- display mode switching via `SetDisplayMode`
- fullscreen swap behavior tied to DX7-era assumptions

Using a popup window avoids forcing a display mode change and generally gives more stable modern Windows behavior.

## Recommended Upgrade Target

If the renderer is ever upgraded within the same broad family, the practical target is **Direct3D 9**, not Direct3D 8.

### Why not DX8

DX8 is mostly a transition API and offers little long-term benefit today.

### Why DX9

DX9 is the most practical endpoint for this style of renderer because:

- it still supports a relatively compatible fixed-function style migration path
- it has better tooling and compatibility than DX8
- modern wrappers and compatibility layers target DX9 much more often
- it is a more worthwhile stop if the goal is modernization without jumping straight to D3D11/Vulkan

## Expected Scope of a DX9 Port

A DX9 migration would be a **real renderer port**, not a header swap.

The current code relies on old DirectDraw/Direct3D 7 concepts for:

- surface creation
- cooperative levels
- fullscreen/windowed transitions
- display mode switching
- backbuffer and z-buffer creation
- enumeration of drivers/modes/formats
- older device capability assumptions

A DX9 port would likely require replacing or redesigning:

- DirectDraw usage
- surface/backbuffer creation paths
- mode switching and presentation setup
- device creation and reset/lost-device handling
- texture and z-buffer creation paths
- parts of capability enumeration

## Practical Recommendation

Short term:

- keep the renderer on the existing DX7-class backend
- improve behavior incrementally, such as borderless/window handling
- preserve compatibility with the existing rendering architecture

Medium term:

- if a renderer API upgrade is pursued, go **straight to DX9**
- do not spend effort on a DX8 stopover

Long term:

- if a more ambitious modernization is desired, a newer API like D3D11/OpenGL/Vulkan would be better than DX9, but that is a much larger rewrite

## Summary

- **Current renderer class:** DX7
- **Best same-family modernization target:** DX9
- **DX8 recommendation:** skip it
- **Risk level of upgrade:** significant renderer port, not a minor change
