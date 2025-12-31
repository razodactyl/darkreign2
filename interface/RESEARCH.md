# UI Scaling Research Notes

## Overview
Dark Reign 2's UI system was designed for 640x480. We're implementing scaling support for higher resolutions (up to 4K) using `IFace::GetScale()` which returns `ScreenWidth / 640.0`.

## Key Concepts

### Coordinate Systems
- **Design-space units**: Original values from config files, based on 640x480
- **Screen-space pixels**: Actual pixel coordinates after scaling

### Key Data Members (IControl::Geometry)
- `geom.pos` / `geom.size`: Legacy fields, kept in sync with unscaled values
- `geom.unscaledConfigPos` / `geom.unscaledConfigSize`: Design-space values from config
- `pos` / `size`: Runtime screen-space pixel values (scaled)

### Scaling Pipeline
1. Config files specify design-space values
2. `Setup()` stores design values in `unscaledConfigPos`/`unscaledConfigSize` via `SetGeomPos()`/`SetGeomSize()`
3. `AdjustGeometry()` (called on every `Activate()`) scales config values to screen pixels
4. `SetSize()`/`SetPos()` take pixel values and reverse-scale to store config (for runtime repositioning)

### Font Scaling (IMPLEMENTED)
- Bitmap fonts are rendered at scaled sizes using integer scaling for pixel-perfect results
- `Font::GetFontScale()` returns `PixelScale::GetIntegerScale()` (floor of UI scale, clamped 1-4)
- `Font::Width()`, `Font::Height()`, `Font::AvgWidth()` return **scaled** values (native * fontScale)
- `Font::Draw()` scales glyph positions, sizes, shadow offset, and character advance by fontScale
- `Font::NativeHeight()`, `Font::NativeAvgWidth()` provide unscaled values for internal use
- Callers (ICEdit, DrawCtrlText, etc.) receive scaled metrics and don't need to scale manually

### PixelScale System (NEW)
Centralized pixel art scaling in `pixelscale.h` / `pixelscale.cpp`:
- `PixelScale::Init()` - Called on startup and resolution change
- `PixelScale::GetIntegerScale(maxScale)` - Returns `floor(IFace::GetScale())` clamped to [1, maxScale]
- `PixelScale::GetScaleRemainder()` - Fractional part after integer scaling
- `PixelScale::ScaleDimension(value)` - Scale a size by integer factor
- `PixelScale::ScalePosition(value, centerRemainder)` - Scale a position, optionally centering remainder
- `PixelScale::Unscale(screenValue)` - Convert screen-space back to design-space
- `PixelScale::GetScaledUV()` - Get UV coordinates with half-texel offset for point filtering
- `PixelScale::GetPixelPerfectRect()` - Calculate pixel-aligned destination rectangle

**Pixel Art Scaling Algorithms** (from Wikipedia):
- `NEAREST` - Simple pixel duplication, fastest
- `SCALE2X` (EPX/AdvMAME2x) - Edge-smoothing 2x, no new colors introduced
- `SCALE3X` (AdvMAME3x) - Edge-smoothing 3x, no new colors
- `EAGLE` - Simple 2x with corner smoothing
- `HQ2X` / `HQ3X` - High quality with YUV color comparison and anti-aliasing (introduces new colors)

**Current Usage:**
- Fonts use `Scale2x` (preserves hard edges, no anti-aliasing)
- UI textures: Experimental `PIXELSCALE_HQ_UI` flag (currently disabled due to pixel format issues)

## Files Modified

### icontrol.cpp
- `AdjustGeometry()`: Scales config values to pixels
- `AutoSize()`: Calculates content size (font dimensions already scaled by Font methods)
- `SetupAlignment()`: Handles cross-parent alignment with coordinate conversion
- `SetSize()`/`SetPos()`: Take pixel values, reverse-scale to config
- `SetGeomSize()`/`SetGeomPos()`: Set design-space values directly
- `GetAdjustmentRect()`: Returns scaled border sizes
- `Setup()` Position handling: Sets `pos` directly without calling `SetPos()` to avoid corrupting `unscaledConfigPos`

### pixelscale.h / pixelscale.cpp (NEW)
- Centralized pixel art scaling system
- Integer scale calculation with caching
- Helper functions for scaling dimensions, positions, UVs
- Algorithm selection infrastructure for future enhancement

### font.cpp (IMPLEMENTED)
- `Font::GetFontScale()`: Static method returning `PixelScale::GetIntegerScale()` for integer scaling
- `Font::Width(s, len)`: Returns scaled width (sums native widths then multiplies by fontScale)
- `Font::Width(c)`: Returns scaled width of single character
- `Font::Height()`: Returns scaled height (`fontHeight * GetFontScale()`)
- `Font::AvgWidth()`: Returns scaled average width (`avgWidth * GetFontScale()`)
- `Font::Draw()`: Scales glyph rect positions (x0,y0,x1,y1), shadow offset, and character advance

### font.h
- Added `#include "pixelscale.h"` for PixelScale access
- Added `static S32 GetFontScale()` declaration
- Changed `Height()` and `AvgWidth()` from inline to declarations (implementations in .cpp)
- Added `NativeHeight()` and `NativeAvgWidth()` inline methods for unscaled access

### iface.cpp
- `Init()`: Calls `PixelScale::Init()` after metrics setup
- `OnModeChange()`: Calls `PixelScale::Init()` when resolution changes

### icedit.cpp
- Uses `Font::Width()` and `Font::Height()` which now return scaled values
- `GetCharAt()`: Pixel-to-character conversion works with scaled font metrics
- `SetCaretPos()`: Caret positioning uses scaled `Width()` and `AvgWidth()`
- `DrawSelf()`: Selection rectangles and caret use scaled font dimensions

### iface_util.cpp
- `TextureSkin::Render()`: Scales skin piece metrics for window borders/backgrounds
- `TextureInfo::UpdateUV()`: Handles texture display modes (TM_CENTRED, TM_STRETCHED, etc.)

### icmenu.cpp
- `ICMenu::Activate()`: Calculates menu button layout

### client_squadcontrol.cpp
- `DrawSelf()`: Scales `number` position, `health` area, bar dimensions from design-space

### common_stats.cpp
- `Stats::Activate()`: Converts `GetSize()` to design-space before layout calculations
- `Stats::Button`: Uses `SetGeomPos()`/`SetGeomSize()` with design-space area values
- `QueryResults::DrawSelf()`: Scales column offset, width, and padding to screen-space
- `QueryHeading::DrawSelf()`: Scales text padding to screen-space

### iface_messagebox.cpp
- `MsgBox()`: Converts font dimensions (`Font::Width()`, `Font::Height()`) from screen-space to design-space before use with `SetGeomSize()`

### multiplayer_controls_earth.cpp
- `RenderBitmap()`: Now takes explicit size parameter for scaled rendering
- `DrawSelf()`: Scales bitmap sizes, player location coordinates, info box dimensions
- `HandleEvent()`: Unscales mouse coordinates for longitude/latitude calculation

## Known Issues & Fixes

### Issue: Buttons growing on resolution change
**Cause**: `AdjustGeometry()` with AUTOSIZE adds `baseWidth` (scaled config size) to autosize result. If config size was set from previous scaled value, it accumulates.
**Fix**: Reset `SetGeomSize(0, 0)` before autosizing in `ICMenu::Activate()`

### Issue: Font dimensions not scaled in AutoSize (IMPLEMENTED)
**Original Cause**: `Font::Width()` and `Font::Height()` returned unscaled pixel values.
**Solution**: Font methods now return scaled values using `PixelScale::GetIntegerScale()`:
- `Font::Width()` returns `nativeWidth * GetFontScale()`
- `Font::Height()` returns `fontHeight * GetFontScale()`
- `Font::AvgWidth()` returns `avgWidth * GetFontScale()`
- `Font::Draw()` renders glyphs at scaled positions and sizes
- `AutoSize()` and other callers receive correct screen-space dimensions automatically

### Issue: Cross-parent alignment not working (dropdowns)
**Cause**: Changed detection logic from `!ctrl->IsChild(this)` to `alignTo->parent != parent`
**Fix**: Restored original `IsChild` logic

### Issue: Map preview not showing
**Cause**: Scaled TM_CENTRED texture larger than control, causing negative offsets
**Fix**: Reverted TM_CENTRED to display at native pixel size

## Current Investigation: Host Configuration Screen

### Symptoms
- Layout not correct
- Need to identify which controls are affected

### Config Location
`C:\Games\Dark Reign 2\unpacked\base\configs\interface\multiplayer\interface_setup_base.cfg`

### Control Hierarchy
- `GameConfigHostOptions` (MultiPlayer::HostConfig extends Game::MissionSelection extends ICWindow)
  - `PlacementTitle` (Static) - `Geometry("Bottom"); Pos(5, -35); Size(155, 15)`
  - `PlacementList` (DropList) - `Align("^"); Geometry("HInternal", "Bottom"); Pos(0, 5); Size(155, 20)`
  - `DifficultyTitle` (Static) - `Geometry("Right", "Top"); Pos(-5, 5); Size(155, 15)`
  - `DifficultyList` (DropList) - `Align("^"); Geometry("HInternal", "Bottom"); Pos(0, 5); Size(155, 20)`
  - etc.

### Layout Pattern
- Uses `Align("^")` to align to previous sibling
- Uses `Geometry("Bottom")` to position at bottom edge of align target
- Uses `Pos(0, 5)` as offset (5 pixels gap between stacked controls)

### Potential Issues
1. The `Pos(-5, 5)` uses negative offset - need to verify this scales correctly
2. `Geometry("Bottom")` combined with `Align("^")` - stacking behavior
3. Controls have fixed `Size(155, 15)` which should scale

### Issue Found: ICWindow::PostConfigure not updating unscaledConfigSize (FIXED)
**Symptom**: Controls with `GEOM_RIGHT` and `GEOM_BOTTOM` inside windows with `STYLE_ADJUSTWINDOW` were positioned incorrectly. For example, `ReturntoMain` button in Options dialog was at (365,360) instead of (355,335).
**Root Cause**: `ICWindow::PostConfigure()` adjusts `size` and `geom.size` for the titlebar/border, but didn't update `geom.unscaledConfigSize`. When `AdjustGeometry()` ran later, it used the unadjusted `unscaledConfigSize` to calculate `designClient`, resulting in wrong parent dimensions for child positioning.
**Fix**: In `ICWindow::PostConfigure()`, also update `geom.unscaledConfigSize` with the unscaled adjustment values:
```cpp
F32 scale = IFace::GetScale();
F32 invScale = (scale > 0.0f) ? (1.0f / scale) : 1.0f;
geom.unscaledConfigSize.x += S32(F32(adjustX) * invScale);
geom.unscaledConfigSize.y += S32(F32(adjustY) * invScale);
```
**Files Modified**: `icwindow.cpp`

### Issue Found: Options menu Selector buttons offset
**Symptom**: Selector control at `pos=(0,10)` instead of expected `pos=(10,10)`
**Cause**: `GEOM_KEEPVISIBLE` (0x200000) + `GEOM_PARENTWIDTH` (0x10) combination. The control fills the parent width, so there's no room for X offset.
**Fix Applied**: Skip KEEPVISIBLE X clamping when PARENTWIDTH is set (the offset is meaningless for full-width controls).
**Status**: Not a bug - this is the intended behavior of GEOM_KEEPVISIBLE

### Issue: Multiplayer Setup Chat Dialogs Overlapping
**Symptom**: `GameConfigChat` appears too high, overlapping with `GameConfigWonChat`
**Analysis**: 
- `GameConfigWonChat`: `pos=(620,10)` ends at Y=310
- `GameConfigChat`: `pos=(626,160)` starts at Y=160 (overlaps!)
- Design values show this overlap exists even at 640x480 (WonChat Y=5-155, Chat Y=80)
- This suggests runtime logic should show only one at a time, or there's missing alignment
**Status**: Likely original design issue or missing runtime visibility logic - not a scaling bug

### Issue: Multiplayer Map Preview Not Displaying (FIXED)
**Symptom**: Preview control exists with correct size (256x256) but texture appeared small/centered
**Cause**: Map preview used `TM_CENTRED` mode which displays texture at native 128x128 pixels, not filling the scaled 256x256 control
**Fix**: Changed to `TM_STRETCHED` in:
- `game/game_missionselection.cpp` - `ValidatePreview()`
- `game/game_campaignselection.cpp` - `ValidatePreview()`
- `multiplayer/multiplayer_controls_mission.cpp` - `DrawSelf()`

### Issue: Start Location Markers Not Scaled (FIXED)
**Symptom**: Start location markers on map preview appeared in wrong positions
**Cause**: Hardcoded `128.0f` multiplier for coordinates assumed 128x128 preview size
**Fix**: Changed to use `pi.client.Width()/Height()` for coordinate scaling and `IFace::GetScale()` for marker size in `multiplayer_controls_mission.cpp`

### Issue: Instant Action Dialog Alignment
**Symptom**: `GameConfigMission` and `GameConfigPlayers` panels not vertically aligned
**Analysis**:
- `GameConfigMission`: `pos=(646,140)` with `flags=0x00040009` (GEOM_RIGHT | GEOM_VCENTRE | GEOM_VINTERNAL)
  - Design: `pos=(5,-20)` - the `-20` Y offset positions it 40 scaled pixels above center
- `GameConfigPlayers`: `pos=(36,180)` with `flags=0x0000000C` (GEOM_HCENTRE | GEOM_VCENTRE)
  - Design: `pos=(-152,0)` - no Y offset, exactly at vertical center
**Status**: This is by design in the config files, not a scaling bug. The original designers intentionally positioned these controls at different Y offsets. Fix would require modifying the `.cfg` files.

### Issue: Tooltip Rendering Offscreen (FIXED)
**Symptom**: Tooltips appeared offscreen or at wrong position instead of at mouse cursor
**Cause**: `ActivateTip()` was setting `c->pos += Input::MousePos()` BEFORE calling `Activate()`. But `Activate()` calls `AdjustGeometry()` which resets `pos` to zero and recalculates it from geometry flags, overwriting the mouse position.
**Fix**: Reordered to call `Activate()` first, then `MoveTo(Input::MousePos())` after geometry is calculated.
**Additional Bug Found**: `MoveTo()` had a typo on line 2108 - `pos.x = IFace::ScreenHeight() - size.y` should be `pos.y = ...`. This caused incorrect Y clamping when tooltip went off bottom of screen.

### Issue: Text Selection Background Too Large (FIXED)
**Symptom**: When selecting text in edit controls, the selection background was larger than the actual rendered text
**Root Cause**: Font metrics needed to return scaled values for proper UI layout
**Solution**: Implemented font scaling in `Font` class:
- `Font::Width()`, `Font::Height()`, `Font::AvgWidth()` now return scaled values via `GetFontScale()`
- `Font::Draw()` scales glyph positions and character advance internally
- `ICEdit` uses scaled font metrics directly - no additional scaling needed
- `GetCharAt()`, `SetCaretPos()`, `DrawSelf()` all work correctly with scaled metrics

### Issue: Squad Control Health Bars Not Scaled (FIXED)
**Symptom**: Health bars and squad number in the grouped units UI were not positioned/sized correctly
**Cause**: `SquadControl::DrawSelf()` used `number` and `health` positions directly from config without scaling
**Fix**: Added `IFace::GetScale()` multiplier for:
- Squad number text position
- Health bar area coordinates
- Bar height, spacing, and triangle indicator dimensions

### Issue: Earth Map Not Scaled (FIXED)
**Symptom**: "Where on Earth is everyone" world map displayed at native 512x256 instead of scaled size
**Cause**: `RenderBitmap()` used bitmap's native `Width()`/`Height()` without scaling
**Fix**: 
- Modified `RenderBitmap()` to take explicit size parameter
- Scale bitmap rendering size by `IFace::GetScale()`
- Scale all coordinate calculations (player locations, mouse position conversion, info boxes)

### Issue: Controls Migrating on Dialog Reopen (FIXED)
**Symptom**: Latitude/Longitude controls in Earth dialog moved up each time the dialog was closed and reopened
**Cause**: During `Setup()`, after `SetGeomPos(x, y)` stored the design-space position, `SetPos(x*scale, y*scale)` was called which reverse-scaled and overwrote `unscaledConfigPos` with a slightly different value due to rounding. On each `Activate()`, `AdjustGeometry()` would recalculate from the corrupted config, accumulating error.
**Fix**: In `Setup()` Position handling, set `pos.x` and `pos.y` directly instead of calling `SetPos()`, preserving the correct `unscaledConfigPos` set by `SetGeomPos()`.

### Issue: Menu Button Auto-Sizing Reset by IControl::Activate (FIXED)
**Symptom**: Menu buttons (Create Game, Join Room, Login, etc.) appeared tiny (20x10) instead of auto-sized to fit text
**Root Cause**: `ICMenu::Activate()` auto-sizes children via `AdjustGeometry()`, but then calls `IControl::Activate()` which iterates children and calls `Activate()` on each. Each child's `Activate()` calls `AdjustGeometry()` again, which resets size back to `geom.unscaledConfigSize` (the tiny config value).
**Fix**: 
1. Store calculated child sizes/positions before calling `IControl::Activate()`
2. After `IControl::Activate()` completes, restore children using new `SetScreenSize()` and `SetScreenPos()` methods
3. These methods set screen-space values without corrupting `geom.unscaledConfigPos/Size`
4. `SetScreenSize()` also updates `paintInfo.window` and `paintInfo.client`
**Files Modified**: `icmenu.cpp`, `icontrol.cpp`, `icontrol.h`

### Issue: Menu Position Wrong After Size Calculation (FIXED)
**Symptom**: Menus with `GEOM_HCENTRE` or `GEOM_BOTTOM` positioned incorrectly after auto-sizing
**Cause**: After restoring the menu's calculated size, position wasn't recalculated for centering flags
**Fix**: In `ICMenu::Activate()`, after restoring size, recalculate position for `GEOM_HCENTRE`, `GEOM_VCENTRE`, `GEOM_RIGHT`, and `GEOM_BOTTOM` flags
**Files Modified**: `icmenu.cpp`

### Issue: Menu Buttons Render Incorrectly When Not Hovered (FIXED)
**Symptom**: Menu buttons render at correct position and size when hovered, but appear wrong when inactive
**Cause**: `ICButton::AdjustGeometry()` calculates `clientRects[BS_UP]` and `clientRects[BS_DOWN]` from `paintInfo.client`. When menu restores button sizes after `IControl::Activate()`, the button's `clientRects` were still using the old (wrong) values.
**Fix**: 
1. Added virtual `OnScreenSizeChanged()` method to `IControl` (empty default implementation)
2. `SetScreenSize()` now calls `OnScreenSizeChanged()` after updating size and paintInfo
3. `ICButton` overrides `OnScreenSizeChanged()` to call `UpdateClientRects()`
4. Extracted `UpdateClientRects()` from `ICButton::AdjustGeometry()` for reuse
**Files Modified**: `icontrol.h`, `icontrol.cpp`, `icbutton.h`, `icbutton.cpp`

### Issue: Aligned Menu Positioned Incorrectly (FIXED)
**Symptom**: `PlayerMenu` (Ignore/Unignore buttons) overlapped with `Menu` (Logout/Back buttons) instead of appearing above it
**Cause**: When a menu aligns to another control, the alignment is calculated during `AdjustGeometry()` using the menu's config size (tiny). After auto-sizing, the menu has a different height, but the position was calculated with the wrong height.
**Fix**: In `ICMenu::Activate()`, after restoring the calculated size, recalculate alignment position if `alignTo` is set. This uses the correct auto-sized dimensions for positioning.
**Files Modified**: `icmenu.cpp`

### Issue: Alignment Target Not Fully Sized (FIXED)
**Symptom**: When control A aligns to menu B, it gets B's config size instead of auto-sized dimensions
**Cause**: `SetupAlignmentDesign()` called `AdjustGeometry()` on inactive align targets, but menus calculate their size in `Activate()`, not `AdjustGeometry()`
**Fix**: Changed `SetupAlignmentDesign()` to call `Activate()` instead of `AdjustGeometry()` on inactive align targets
**Files Modified**: `icontrol.cpp`

### Issue: Popup Menu Positioned Off-Screen (FIXED)
**Symptom**: `Studio::ToolsPopup` menu appeared at `pos=(-315,360)`, off-screen to the left
**Cause**: The popup aligns to the `Tools` button but lacks `GEOM_HINTERNAL` flag. Without it, the alignment formula is `pos.x = alignPos.x - size.x = 0 - 315 = -315`. The `GEOM_KEEPVISIBLE` flag should clamp this to screen bounds, but it was only applied in `AdjustGeometry()`, not after our position recalculation in `ICMenu::Activate()`.
**Fix**: Apply `GEOM_KEEPVISIBLE` clamping after alignment recalculation in `ICMenu::Activate()`:
```cpp
if (geom.flags & GEOM_KEEPVISIBLE)
{
    pos.x = Clamp<S32>(0, pos.x, parentWidth - size.x);
    pos.y = Clamp<S32>(0, pos.y, parentHeight - size.y);
}
```
**Files Modified**: `icmenu.cpp`

### Issue: Resolution Change Not Updating UI (FIXED)
**Symptom**: When changing resolution, UI controls and fonts didn't rescale properly
**Cause**: `PixelScale::Init()` wasn't being called on mode change, and controls weren't recalculating geometry
**Fix**: 
1. Call `PixelScale::Init()` in `IFace::OnModeChange()` before `FontSys::OnModeChange()`
2. Call `AdjustGeometry()` in `IControl::HandleEvent` for `DISPLAYMODECHANGED`
3. `ICMenu` handles `DISPLAYMODECHANGED` by deactivating and reactivating to re-layout children
**Files Modified**: `iface.cpp`, `icontrol.cpp`, `icmenu.cpp`

### Feature: Font Texture Pre-Scaling (IMPLEMENTED)
**Purpose**: Crisp pixel-art style fonts at high resolutions instead of blurry GPU-scaled fonts
**Implementation**: 
1. `fontTextureScale` static variable tracks the scale factor used when loading fonts
2. Font textures are created at `128 * scale` size instead of 128
3. Each glyph pixel is written as a `scale x scale` block (nearest-neighbor scaling)
4. UV coordinates are normalized so they work correctly with scaled textures
5. `Font::GetFontScale()` returns `fontTextureScale` so rendering uses correct size
**Files Modified**: `font.cpp`

### Issue: PIXELSCALE_SCALE2X_UI Causes Texture Corruption (KNOWN ISSUE)
**Symptom**: UI textures become progressively more corrupted/low-resolution when loading games
**Cause**: `ScaleBitmapUI()` tracks scaled bitmaps by pointer, but bitmap recreation after `Release()`/`Create()` may reuse memory addresses or the tracking doesn't persist correctly across bitmap reloads
**Status**: DISABLED - the `PIXELSCALE_SCALE2X_UI` define is commented out
**Note**: Font pre-scaling is handled separately in `font.cpp` and works correctly
**Files Modified**: `pixelscale.h` (disabled the define)

### Issue: Window Skin/Border Scaling (FIXED)
**Symptom**: At higher resolutions, window borders/titlebars appeared too small - tiny corner pieces in large windows.

**Root Cause**: `TextureSkin::Render()` used piece dimensions (`pos`, `textureSize`, `size`) directly from config files without scaling. These values are in design space (640x480 base), but `pi.window` and `pi.client` are already in screen space.

**Fix**: Scale all piece dimensions by `IFace::GetScale()` in `TextureSkin::Render()`:
```cpp
F32 scale = IFace::GetScale();
auto scalePoint = [scale](const Point<S32>& p) -> Point<S32> {
    return Point<S32>(S32(F32(p.x) * scale), S32(F32(p.y) * scale));
};
// For each piece:
Point<S32> scaledPos = scalePoint(piece.pos);
Point<S32> scaledTexSize = scalePoint(piece.textureSize);
Point<S32> scaledSize = scalePoint(piece.size);
```

**Verification**: Compared with original DR2 build at 640x480 - the visual appearance of menu borders now matches the original game identically. The stretched/warped appearance is how the original skins were designed.

**Files Modified**: `iface_util.cpp`

### Issue: Apply Button Layout in Options Video Tab (FIXED - RE-APPLIED)
**Symptom**: Apply button and other controls with `GEOM_RIGHT`/`GEOM_BOTTOM` inside windows were positioned incorrectly after code was accidentally reverted during debugging.

**Root Cause**: The fix for `ICWindow::PostConfigure()` updating `geom.unscaledConfigSize` was lost during attempts to fix the skin warping issue.

**Fix Re-Applied**: In `ICWindow::PostConfigure()`, when adjusting window size for titlebar/border with `STYLE_ADJUSTWINDOW`, also update `geom.unscaledConfigSize`:
```cpp
if (windowStyle & STYLE_ADJUSTWINDOW)
{
    ClipRect r = GetAdjustmentRect();
    S32 adjustX = r.p0.x - r.p1.x;
    S32 adjustY = r.p0.y - r.p1.y;

    size.x += adjustX;
    size.y += adjustY;
    geom.size.x += adjustX;
    geom.size.y += adjustY;

    // Also update unscaledConfigSize so AdjustGeometry uses the correct values
    // GetAdjustmentRect returns scaled values, so we need to unscale them
    F32 scale = IFace::GetScale();
    F32 invScale = (scale > 0.0f) ? (1.0f / scale) : 1.0f;
    geom.unscaledConfigSize.x += S32(F32(adjustX) * invScale);
    geom.unscaledConfigSize.y += S32(F32(adjustY) * invScale);
}
```

**Why This Matters**: Without updating `unscaledConfigSize`, `AdjustGeometry()` calculates `designClient` from the unadjusted config size, resulting in wrong parent dimensions. Child controls with `GEOM_RIGHT` or `GEOM_BOTTOM` use these parent dimensions for positioning, so they end up in the wrong place.

**Related Fixes Also Re-Applied**:
1. `IControl::GetAdjustmentRect()` - scales border/shadow metrics
2. `ICWindow::GetAdjustmentRect()` - scales title height
3. `ICWindow::PostConfigure()` - uses `SetGeomSize()`/`SetGeomPos()` for titlebar and close button (design-space values)

**Files Modified**: `icwindow.cpp`, `icontrol.cpp`

## Geometry Flags Reference

Common flags used in control positioning (from `icontrol.h`):
```
GEOM_RIGHT        (1 << 0)   0x0001  - Align to right edge
GEOM_BOTTOM       (1 << 1)   0x0002  - Align to bottom edge
GEOM_HCENTRE      (1 << 2)   0x0004  - Horizontally centered
GEOM_VCENTRE      (1 << 3)   0x0008  - Vertically centered
GEOM_PARENTWIDTH  (1 << 4)   0x0010  - Size to parent width
GEOM_PARENTHEIGHT (1 << 5)   0x0020  - Size to parent height
GEOM_SQUARE       (1 << 14)  0x4000  - Maintain square aspect
GEOM_AUTOSIZEX    (1 << 15)  0x8000  - Auto-size width to content
GEOM_AUTOSIZEY    (1 << 16) 0x10000  - Auto-size height to content
GEOM_HINTERNAL    (1 << 17) 0x20000  - Internal horizontal alignment
GEOM_VINTERNAL    (1 << 18) 0x40000  - Internal vertical alignment
GEOM_KEEPVISIBLE  (1 << 21) 0x200000 - Clamp to stay visible in parent
```

## Debug Logging

### Layout Dump Command (Recommended)
Use the console command to dump UI layout to a file:
```
iface.dumplayout              # Dump entire UI hierarchy
iface.dumplayout ControlName  # Dump specific control and children
```
Output is written to `ui_layout.log` in the game directory.

### Per-Frame Logging (Disabled)
The following log channels have been disabled to prevent spam:
- `[ALIGN]` - Was logging every alignment calculation
- `[IFACE_ADJUST]` - Was logging every geometry adjustment  
- `[WIN_DRAW]` - Was logging every frame

### Still Active
- `[WIN_POSTCFG]` - Logs once per window post-configuration (useful)

## Key Learnings

### SetPos vs SetGeomPos
- `SetGeomPos(x, y)`: Sets design-space position directly in `unscaledConfigPos` - use for config file values
- `SetPos(x, y)`: Sets screen-space position AND reverse-scales to update `unscaledConfigPos` - use for runtime repositioning (e.g., menus)
- **Critical**: During `Setup()`, don't call `SetPos()` after `SetGeomPos()` as it will corrupt the config with rounding errors

### Font Scaling Strategy (IMPLEMENTED)
- Integer scaling via `PixelScale::GetIntegerScale()` ensures pixel-perfect rendering without sub-pixel artifacts
- All font metric methods (`Width()`, `Height()`, `AvgWidth()`) return scaled values
- `Font::Draw()` scales glyph positions and character advance internally
- Callers receive screen-space values and don't need to scale font metrics manually
- `NativeHeight()` and `NativeAvgWidth()` provide unscaled values when needed

### Custom Control DrawSelf Pattern
Controls with custom `DrawSelf()` that use hardcoded positions/sizes from config need to:
1. Get `scale = IFace::GetScale()` at start of `DrawSelf()`
2. Multiply all design-space coordinates by `scale` before rendering
3. For mouse input, divide screen coordinates by `scale` to get design-space values

## Known Controls Needing Scaling

These controls have custom rendering that required scaling fixes:
- [x] `Client::SquadControl` - Health bars, squad numbers
- [x] `MultiPlayer::Controls::Earth` - World map, player locations
- [x] `MultiPlayer::Controls::Mission` - Start location markers (previously fixed)
- [x] `Common::Stats` - End-game statistics grid and detail dialogs
- [x] `IFace::MsgBox` - Confirm dialog boxes
- [ ] Other controls with custom `DrawSelf()` may need similar treatment

## Bitmap Pixel Format Research

### The Pix Structure (`graphics/bitmap.h`)
The `Pix` struct defines the pixel format for bitmaps, which varies based on video hardware:

```cpp
struct Pix {
    DDPIXELFORMAT pixFmt;           // DirectDraw pixel format
    U32 rMask, rShift, rScale, rScaleInv;  // Red channel
    U32 gMask, gShift, gScale, gScaleInv;  // Green channel
    U32 bMask, bShift, bScale, bScaleInv;  // Blue channel
    U32 aMask, aShift, aScale, aScaleInv;  // Alpha channel
    
    U32 MakeRGBA(U32 r, U32 g, U32 b, U32 a = 255) const {
        return ((r >> rScaleInv) << rShift)
             + ((g >> gScaleInv) << gShift)
             + ((b >> bScaleInv) << bShift)
             + ((a >> aScaleInv) << aShift);
    }
};
```

### Common Pixel Formats
- **RGB565** (16-bit): R=5 bits, G=6 bits, B=5 bits, no alpha
- **ARGB1555** (16-bit): A=1 bit, R=5, G=5, B=5
- **ARGB8888** (32-bit): A=8, R=8, G=8, B=8 (standard)
- **BGRA8888** (32-bit): B=8, G=8, R=8, A=8 (DirectX default)

### Extracting RGBA from Bitmap Pixel
To properly extract color components from a bitmap pixel:
```cpp
// Given: U32 pixel from Bitmap::GetPixel()
// And: Pix* pixForm from bitmap
U8 r = (pixel & pixForm->rMask) >> pixForm->rShift << pixForm->rScaleInv;
U8 g = (pixel & pixForm->gMask) >> pixForm->gShift << pixForm->gScaleInv;
U8 b = (pixel & pixForm->bMask) >> pixForm->bShift << pixForm->bScaleInv;
U8 a = (pixel & pixForm->aMask) >> pixForm->aShift << pixForm->aScaleInv;
```

### Why PIXELSCALE_HQ_UI Failed
The `ScaleBitmapHQ()` function assumed ARGB8888 format:
```cpp
// WRONG - assumes fixed format
a = (color >> 24) & 0xFF;
r = (color >> 16) & 0xFF;
g = (color >> 8) & 0xFF;
b = color & 0xFF;
```

But bitmaps can be in any format (RGB565, BGRA, etc.). The fix requires:
1. Get `pixForm` from the bitmap
2. Use the mask/shift values to extract RGBA
3. After scaling, use `pixForm->MakeRGBA()` to convert back

### Font Scaling Works Because...
Font textures are created fresh with known format:
```cpp
// In Font::Read() - we control the pixel format
U32 color = texture->MakeRGBA(0xFF, 0xFF, 0xFF, alpha);
texture->PutPixel(x, y, color, &clip);
```
The source data is 8-bit alpha values that we convert ourselves, so we know the format.

### HQ UI Scaling Status (PIXELSCALE_HQ_UI)

**Implementation completed but disabled** - pixel format handling now works correctly:
- ✅ Pixel format conversion using `Pix` masks (ExtractRGBA/PackRGBA)
- ✅ Canonical ARGB8888 conversion for scaling algorithms
- ✅ Bitmap tracking to prevent double-scaling of cached textures
- ✅ Scale factor return value for correct UV/pixel coordinate adjustment

**Why it's disabled:**
1. **HQ2X not suitable for UI** - Anti-aliasing makes sharp UI edges look blurry/soft
2. **Scale2x might work better** - Preserves hard edges, but still experimental
3. **Some textures don't scale correctly** - Texture atlas UV issues, cached texture problems
4. **Performance overhead** - CPU-based scaling at load time adds delay

**Better approaches to consider:**
1. **GPU-based scaling** - Let the GPU handle scaling with appropriate filter modes
2. **Pre-scaled assets** - Ship 2x/4x versions of UI textures
3. **Vector/procedural UI** - Generate UI elements at runtime (major rewrite)
4. **Selective scaling** - Only scale certain texture types (fonts work well with Scale2x)

**Current state:** Font scaling with Scale2x works well. UI texture scaling disabled.

## Scaling Audit (2024-11-29)

### Files Audited

| File | Status | Issues Found |
|------|--------|--------------|
| `icbutton.cpp` | ✅ OK | Uses `GetMetric()` for dropshadow (already scaled in GetAdjustmentRect) |
| `icslider.cpp` | ✅ OK | Uses `GetMetric()` for shadow, `paintInfo.client` for ranges |
| `iclistbox.cpp` | ✅ OK | Uses `GetMetric(IFace::SLIDER_WIDTH)` (scaled in GetAdjustmentRect) |
| `icstatic.cpp` | ✅ OK | No hardcoded pixel values |
| `icgauge.cpp` | ✅ OK | Uses `paintInfo.client` which is already scaled |
| `ictipwindow.cpp` | ✅ OK | No hardcoded pixel values |
| `icgrid.cpp` | ✅ OK | Programmatic control, uses passed-in screen-space values |
| `icmenu.cpp` | ✅ FIXED | `menuEdge` now scaled in `Activate()` |
| `icwindow.cpp` | ✅ FIXED | `titleHeight` now scaled in `PostConfigure()` and `GetAdjustmentRect()` |
| `icsystembutton.cpp` | ✅ FIXED | Icon drawing offsets now scaled |

### Issues to Fix

#### icmenu.cpp - menuEdge not scaled
**Location**: `ICMenu::Activate()` lines 320-378
**Problem**: `menuEdge` (default 3) is used directly without scaling for menu item positioning
**Fix**: Scale `menuEdge` by `IFace::GetScale()` when calculating offsets

#### icwindow.cpp - titleHeight not scaled
**Location**: `ICWindow::PostConfigure()` lines 104-142
**Problem**: `titleHeight` from `GetMetric()` is used directly for titlebar and close button sizing
**Fix**: Scale `titleHeight` by `IFace::GetScale()` before use

#### icwindow.cpp - GetAdjustmentRect returns unscaled
**Location**: `ICWindow::GetAdjustmentRect()` lines 156-173
**Problem**: Returns `GetMetric(IFace::TITLE_HEIGHT)` without scaling
**Fix**: Scale the title height metrics (note: base class already scales, but this override doesn't)

#### icsystembutton.cpp - hardcoded pixel offsets
**Location**: `DrawLeftIcon()`, `DrawRightIcon()`, `DrawUpIcon()`, `DrawDownIcon()` lines 271-324
**Problem**: Uses hardcoded `+2`, `-2`, `+1` pixel offsets for icon triangles
**Fix**: Scale these offsets by `IFace::GetScale()`

### AdjustGeometry Logic

The `AdjustGeometry` function handles scaling:
- If `unscaledConfigSize` is set (from config), scale those values to screen-space
- Otherwise, use `geom.size` directly (for legacy/programmatic controls)
- Same logic applies to position with `unscaledConfigPos` and `geom.pos`

### IconWindow Scaling (2024-11-29)

Fixed `iconwindow.cpp` to scale icon grid layout:
- `AddIcon()`: Use `SetGeomSize()` to set design-space icon dimensions (AdjustGeometry will scale)
- `GetSlotPosition()`: Scale `gridStart`, `iconSize`, and `iconSpacing` for positioning
- `DrawSelf()`: Scale icon dimensions when rendering blank slots

### ICMenu Size Preservation

Fixed `icmenu.cpp` `Activate()`:
- Menu calculates its size based on children
- Size is preserved after `IControl::Activate()` call (which calls `AdjustGeometry`)
- Don't set `geom.size` directly - only set `size` and preserve it after base activation

### Tooltip Positioning Fix

Fixed `icontrol.cpp` `ActivateTip()`:
- Moved mouse position adjustment AFTER `Activate()` call
- Previously, `AdjustGeometry()` was overwriting the mouse-based position

### scaledRegion for Hit Testing

Added `scaledRegion` member to `IControl` for accurate hit testing on scaled irregular regions:
- Created in `AdjustGeometry()` when `region` exists
- Used in `Find()` and `InWindow()` for hit testing
- Scales design-space polygon points to screen-space

## TODO

- [x] ~~Investigate Host Configuration screen layout~~ (not a scaling bug)
- [x] ~~Check for other controls with custom Activate/layout logic~~ (found and fixed Earth, SquadControl)
- [x] ~~Fix PIXELSCALE_HQ_UI pixel format handling~~ (implemented but disabled - not suitable for UI)
- [x] Add scaledRegion for hit testing on irregular regions
- [x] Add fallback logic in AdjustGeometry for programmatic controls
- [x] Fix icmenu.cpp menuEdge scaling
- [x] Fix icwindow.cpp titleHeight scaling
- [x] Fix icsystembutton.cpp icon offset scaling
- [x] Fix iface_messagebox.cpp font dimension double-scaling
- [x] Fix common_stats.cpp layout mixing coordinate spaces
- [x] Fix common_stats.cpp QueryResults/QueryHeading DrawSelf scaling
- [ ] Verify all hardcoded pixel values are scaled in remaining custom controls
- [ ] Test all UI screens at multiple resolutions
- [ ] Consider in-game HUD elements (health bars, resource displays, minimap)
- [ ] Consider shipping pre-scaled UI assets for high-DPI support

## Recent Fixes (2024-12-31)

### Issue: Message Box Dialog Too Large (FIXED)
**File**: `iface_messagebox.cpp`
**Symptom**: Confirm dialog boxes appeared oversized at higher resolutions
**Root Cause**: Font dimensions (`Font::Width()`, `Font::Height()`) return **scaled** screen-space values, but were passed directly to `SetGeomSize()` which expects **design-space** values. This caused double-scaling.
**Fix**: Convert font dimensions back to design-space before use:
```cpp
F32 scale = IFace::GetScale();
F32 invScale = (scale > 0.0f) ? (1.0f / scale) : 1.0f;
S32 titleWidth = S32(F32(font->Width(title, Utils::Strlen(title))) * invScale);
S32 fontHeightDesign = S32(F32(font->Height()) * invScale);
```

### Issue: Stats Grid Layout Broken (FIXED)
**File**: `common_stats.cpp`
**Symptom**: End-game statistics buttons were mispositioned/overlapping
**Root Cause**: `Stats::Activate()` mixed coordinate spaces:
- `GetSize().x` returns screen-space pixels
- `columnGap`, `rowGap`, `rowHeight` are design-space config values
- Calculation `F32(GetSize().x - columnGap * INFO_MAX)` subtracts design from screen

Additionally, `Stats::Button` constructor called both:
- `SetPos()`/`SetSize()` (screen-space) 
- `SetGeomSize()` (design-space) with the same values

**Fix**: 
1. Convert `GetSize()` to design-space before layout calculations
2. Use only `SetGeomPos()`/`SetGeomSize()` with design-space values in Button constructor

### Issue: Stats Detail Dialog Text Squished (FIXED)
**File**: `common_stats.cpp`
**Symptom**: When clicking stats buttons to see details, text appeared squished at higher scales
**Root Cause**: `QueryResults::DrawSelf()` and `QueryHeading::DrawSelf()` used hardcoded design-space values (`offset = 150`, `width = 48`, padding `5`) directly with screen-space client coordinates.
**Fix**: Scale layout values to screen-space before rendering:
```cpp
F32 scale = IFace::GetScale();
S32 scaledOffset = S32(150.0f * scale);
S32 scaledWidth = S32(48.0f * scale);
S32 scaledPadding = S32(5.0f * scale);
```

## Key Patterns Discovered

### Double-Scaling Anti-Pattern
**Problem**: Passing scaled values to functions that expect design-space values.
**Common Symptom**: UI elements appear too large at high resolutions.
**Examples**:
- `Font::Width()`/`Height()` → `SetGeomSize()` (WRONG - font returns scaled, SetGeomSize expects design)
- `GetSize()` mixed with config values in calculations (WRONG - GetSize is screen-space)

**Solution**: Always convert to the expected coordinate space:
```cpp
// Screen-space to design-space
S32 designValue = S32(F32(screenValue) * invScale);

// Design-space to screen-space  
S32 screenValue = S32(F32(designValue) * scale);
```

### SetPos/SetSize vs SetGeomPos/SetGeomSize
| Method | Input Space | Behavior |
|--------|-------------|----------|
| `SetGeomPos(x,y)` | Design-space | Stores directly in `unscaledConfigPos` |
| `SetGeomSize(w,h)` | Design-space | Stores directly in `unscaledConfigSize` |
| `SetPos(x,y)` | Screen-space | Stores in `pos`, reverse-scales to `unscaledConfigPos` |
| `SetSize(w,h)` | Screen-space | Stores in `size` only (no config update) |

**Rule**: When creating controls programmatically with calculated positions/sizes:
- If calculations are in design-space → use `SetGeomPos()`/`SetGeomSize()`
- If calculations are in screen-space → use `SetPos()`/`SetSize()` but NOT `SetGeomSize()`

### DrawSelf Coordinate Spaces
In `DrawSelf(PaintInfo& pi)`:
- `pi.client` / `pi.window` → **Screen-space** (already scaled)
- `pi.font->Width()`/`Height()` → **Screen-space** (scaled by Font methods)
- Config values (hardcoded offsets) → **Design-space** (must scale before use)

**Pattern for custom DrawSelf**:
```cpp
void DrawSelf(PaintInfo& pi) override
{
    F32 scale = IFace::GetScale();
    
    // Scale any hardcoded design-space values
    S32 scaledMargin = S32(10.0f * scale);
    S32 scaledWidth = S32(100.0f * scale);
    
    // pi.client and font metrics are already screen-space
    pi.font->Draw(pi.client.p0.x + scaledMargin, ...);
}
