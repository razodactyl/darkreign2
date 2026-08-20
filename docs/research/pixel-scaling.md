# Interface scaling and the pixel-art filters

How the game gets from a 640x480 design space to a 4K display, what the
`PixelScale` filters actually do, and how faithful each one is to the algorithm
it is named after.

## Two mechanisms, not one

It is easy to assume "4K support" is one feature. It is two, and they are
independent:

**Layout scaling** is the substantive one. Every control's geometry is authored
in a 640x480 design space in the `.cfg` files. `IFace::GetScale()` returns
`min(width / 640, height / 480)` - the smaller axis, so the UI fits in both -
and `IControl::AdjustGeometry` multiplies design-space values up to screen
space.

The important detail there is `geom.unscaledConfigSize`: the design-space value
is kept alongside the scaled one, so a resolution change re-scales from the
original rather than compounding rounding on what it produced last time.

This is a continuous float scale, and it is gated on `#define HIRES_UI`
(iface.cpp) plus, now, the `-ui4k` switch.

**Texture pre-scaling** is the small one. Bitmaps that will be magnified are
built at an integer multiple of their native size so that texels land on pixel
boundaries. This does not add detail - the source art is whatever resolution it
is - it stops the GPU's bilinear filter from having anything to interpolate,
which is what makes magnified text look soft.

The integer factor is `floor(rawScale)` clamped to `[1, maxScale]`; fonts ask
for a cap of 3.

Fonts and UI textures have separate switches (`-fontup`, `-texup`) and separate
algorithm settings, and both default to nearest. They are split because their
content is not alike: UI art is flat colour with hard edges, which is what the
pixel-art filters were designed for, whereas a glyph is white everywhere and
carries all of its shape in an 8-bit alpha ramp. Fed a ramp, the filters read it
as a stack of diagonals and smooth it further. Tying both to one switch would
mean any choice good for one was imposed on the other.

`PixelScale::GetTextureScale` and `GetFontScale` apply the per-consumer switch;
`GetIntegerScale` is the raw resolution-derived factor with only the master
switch folded in. Consumers should ask for their own.

## What is actually pre-scaled

| | Scaled | Default | Where |
| --- | --- | --- | --- |
| Font glyphs | yes | `nn`, on | `font.cpp`, atlas built at `128 * factor` |
| UI textures | yes | **off** | `iface_util.cpp` via `ScaleBitmapUI` |
| World textures | no | - | nothing outside `interface/` calls PixelScale |

UI textures default to off deliberately. The filters are selectable so they can
be evaluated on real interface art; until that is done the shipping game should
be untouched.

## Why UI texture scaling was disabled, and what it took to enable it

It used to be compiled out behind `PIXELSCALE_SCALE2X_UI`, with a note saying it
"causes texture corruption when bitmaps are reloaded". Two separate faults sat
behind that.

**The tracking could not survive a reload.** `ScaleBitmapUI` remembered which
bitmaps it had scaled in a file-static `std::set<Bitmap*>`. Nothing ever cleared
it - `ClearScaledBitmaps` existed and had no callers - so it held raw pointers to
bitmaps that could be freed and reallocated at the same address. Worse,
`IFace::OnModeChange` reloads every unmanaged bitmap with `ReleaseDD` followed by
`Read`, which restores the picture to native size while leaving the set convinced
it was still 2x. The next lookup reported a factor the pixels no longer had, and
the caller stepped its texture coordinates off the end of the bitmap.

The factor now lives on the bitmap itself, as `Bitmap::UIScale`. It describes
those pixels, so every path that replaces them - both `Create` overloads and
`Read` - resets it. There is no side table to go stale and no pointer to dangle,
and the question "has this been scaled?" is answered by the thing being asked
about.

**The buffer has to go with the factor.** This is the part that is easy to miss,
and it is what made the first attempt at enabling this corrupt the interface on
a resolution change. `ReadPIC`, `ReadBMP` and `ReadTGA` all read into an
existing buffer when there is one, and only call `Create` when there is not -
"read into existing bitmap, truncate bitmap if it will not fit". That is fine
when the buffer came from the same file. A pre-scaled bitmap has a buffer
several times larger than its file, so the reload left the native-size picture
in one corner, the rest of the buffer stale, the dimensions still reporting the
scaled size, and - because `Create` never ran - the factor still claiming the
picture was scaled. Every texture coordinate then addressed a fraction of the
region it should have, which on screen looks like the art has reverted to raw
1:1 texels.

`Bitmap::DropUIScale` frees the buffer along with the factor, which forces the
allocate-to-fit path. It is called from `Read` and, separately, from
`ReLoad`'s no-filename branch - that branch calls the readers directly rather
than going through `Read`, and `Bitmap::Manager::OnModeChange` is exactly the
caller that takes it.

**The caller conflated source region with screen size.** `TextureInfo::pixels`
was multiplied by the scale factor at load. But `pixels` is not only a rectangle
within the source texture: `TM_CENTRED` draws at `pixels.Width()`, and the HUD
sizes its reticle corners, damage corners and bars from
`pixels.p1 - pixels.p0`. Doubling it made those elements physically twice as
large rather than twice as sharp. `TM_TILED` had the same problem by another
route - it derives its repeat from `InvWidth`, so a 2x texture tiled half as
often, at twice the size.

`pixels` now stays in the space the art was authored in, and only the UV
computation steps up by the factor. `Bitmap` gained `UnscaledWidth`,
`UnscaledHeight`, `InvUnscaledWidth` and `InvUnscaledHeight` for the places that
want authored-space figures. Normalised texture coordinates come out identical
either way, which is also why they stay correct through a reload that drops the
bitmap back to native size.

Two smaller things fixed in passing: the old code recreated the bitmap with a
hardcoded `translucent = TRUE`, turning every opaque interface texture
translucent; and it refused outright rather than reducing the factor when the
device's maximum texture size would be exceeded.

## Filter fidelity

`-texup` and `-fontup` each select one of six. They are not all equally
faithful to their names.

### Scale2x, Scale3x, Eagle - faithful

Checked line by line against the published rules; the implementations match.
Scale2x's four corner rules, Scale3x's nine, and Eagle's four are all correct,
including the asymmetric guard terms in Scale3x's edge cases that are easy to
get wrong.

### hq2x, hq3x - approximations, and named misleadingly

These are **not** Maxim Stepin's hqx. Genuine hqx builds a bitmask of the eight
neighbour comparisons and switches on it over 256 hand-tuned cases, with a
family of interpolators at different weights.

What is implemented here is four symmetric corner rules using a YUV perceptual
comparison and a 50/50 blend - closer to "Scale2x with anti-aliasing" than to
hqx. It is a reasonable filter. It is not hqx, and it will round off shapes that
real hqx preserves.

The names are kept because they are what the switches take, but nobody should
read `-texup:hq2x` as "this is hq2x".

Two real defects were found and fixed in them:

- **A dead pattern mask.** `Hq2x` built the 8-bit neighbour mask that real hqx
  switches on, then never read it. Eight comparisons per pixel, discarded, and
  a strong hint to the next reader that the 256-case machinery was in there
  somewhere. It was not.

- **Corner softening.** Each corner had an unguarded
  `else if (!ColorsDifferent(D, B)) p1 = Interp3(E, D, B)`. That fires whenever
  the two edge neighbours merely agree with each other - including when the
  diagonal beyond the corner matches the centre, which is precisely the case
  where the correct output is the centre pixel untouched. Every corner in the
  image was being blended 50% towards its neighbours. For a filter whose entire
  purpose is preserving hard edges under magnification, that is backwards.

`Hq3x` had a third, worse one:

```cpp
if (!ColorsDifferent(B, B))  // Always true, blend with neighbors
    p2 = (!ColorsDifferent(D, B) || !ColorsDifferent(B, F)) ? Interp2(E, B) : E;
```

`B` compared against itself. The comment noticed and shrugged. The damage was
not the dead test but what it guarded: the top-middle output pixel was blended
halfway towards `B` whenever `B` agreed with either horizontal neighbour, which
is true along every straight horizontal run in the image. Straight edges were
being blurred into the row above them at 3x. The four edge pixels now stay as
`E`, matching Scale3x.

### Nearest - the default, and usually the right one

The game's art - fonts especially - is already anti-aliased continuous tone.
The pixel-art filters all assume hard-edged indexed input; fed an alpha ramp
they read the ramp as a stack of diagonals and smooth it further. Nearest
neighbour at an integer factor is exact: it changes nothing except where the
texel boundaries land, which is the whole point of pre-scaling.

Nearest previously only worked at 2x, hardcoded, despite the font path asking
for factors of 1, 2 and 3. It takes an arbitrary factor now.

## Alpha

`Interp2`/`Interp3`/`Interp4` averaged the raw channels, including colour under
transparent texels. Most of the UI art stores black under its transparent
pixels, so any blend touching an edge pulled the result towards black - a dark
fringe around everything the filter smoothed.

They now average in premultiplied-alpha space and un-premultiply on the way
out, so a transparent contributor moves alpha without moving colour.

Font glyphs could never show this: they are white everywhere and carry all
their shape in alpha. Given fonts are the only live consumer, that is presumably
how it survived.

## Composability

The filters are not composable. Running a 2x filter twice is not a 4x filter -
the second pass gets anti-aliased input it was never designed to classify, and
compounds its own mistakes.

`ScaleImageTo` therefore only ever runs one pass. It picks the variant of the
requested algorithm that natively produces the requested factor (Scale2x and
Scale3x are one family, hq2x and hq3x another), and where there is no such
variant - Eagle at 3x, anything at 4x - it falls back to nearest neighbour.
Nearest never distorts; it just does not smooth. That is the honest failure
mode.

## Tests

`tests/pixelscale/` builds the filters standalone against a stubbed `IFace` and
checks the properties that matter:

- Scale2x against its published rules
- every filter leaves a flat field flat
- every filter leaves straight horizontal and vertical edges crisp (the hq3x
  regression)
- corners are rewritten only where a real diagonal cuts them (the `else if`
  regression), and Hq2x agrees with Scale2x where it should
- blending against a transparent texel moves alpha, not colour
- nearest is exact at factors 2, 3 and 4
- family fallback in `ScaleImageTo`, and `ScaleImageWith` honouring the
  algorithm it is handed rather than the global one
- switch parsing, and that `-upscale`, `-ui4k`, `-texup` and `-fontup` are
  independent in the right direction
- `-texup` not reaching the font algorithm, and `-fontup` not reaching the
  general one
- the shipping defaults: texture scaling off, font scaling on at nearest
- texture pre-scaling recording its factor on the bitmap, not re-scaling an
  already-scaled one, and - the regressions that disabled the feature -
  dropping both the factor and the oversized buffer when the bitmap is
  reloaded. The stub Bitmap models the readers' reuse-an-existing-buffer
  behaviour, so removing the fix fails the tests rather than passing quietly.
- translucency surviving the recreate, and the device texture-size limit
  reducing the factor rather than refusing

Run `tests\pixelscale\run.bat`. It is deliberately outside `dr2.sln` so it
cannot break the game build.

## Not done

- UI texture scaling is off by default and wants evaluating on real art before
  that changes. Worth looking at specifically: the flat, hard-edged panel
  borders, which are the closest thing here to what the filters were designed
  for.
- Nothing re-scales textures when the resolution changes at runtime. A mode
  change reloads unmanaged bitmaps at native size and they stay there until the
  interface is reloaded. Layout still rescales, so the art is merely softer, not
  wrong - but it is not what a fresh launch at that resolution would give.
- A faithful hqx would need the upstream 256-case tables. Worth doing only if
  someone wants hqx specifically; on this art it is unlikely to beat nearest.
- `GetScaledUV` and `GetPixelPerfectRect` are exported and unused. They date
  from an earlier approach where the UI sampled with point filtering.
