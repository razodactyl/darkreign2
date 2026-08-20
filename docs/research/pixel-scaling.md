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

| | Scaled | Where |
| --- | --- | --- |
| Font glyphs | yes | `font.cpp`, atlas built at `128 * factor` |
| UI textures | no | `iface_util.cpp`, behind `PIXELSCALE_SCALE2X_UI` |
| World textures | no | nothing outside `interface/` calls PixelScale |

`PIXELSCALE_SCALE2X_UI` is compiled out. The note in `pixelscale.h` is honest
about why: the bitmap tracking does not survive bitmap recreation, so a texture
that gets reloaded is scaled twice, or the tracking set holds a dangling
pointer. That is a real bug and it has not been fixed - the switch plumbing
added alongside this document reaches the call site, but the call site is still
`#ifdef`-ed out. Turning it back on means fixing the tracking first.

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

Run `tests\pixelscale\run.bat`. It is deliberately outside `dr2.sln` so it
cannot break the game build.

## Not done

- `PIXELSCALE_SCALE2X_UI` is still off. The bitmap-recreation tracking needs
  fixing before it can be turned on, and the switch is wired to reach it when
  it is.
- A faithful hqx would need the upstream 256-case tables. Worth doing only if
  someone wants hqx specifically; on this art it is unlikely to beat nearest.
- `GetScaledUV` and `GetPixelPerfectRect` are exported and unused. They date
  from an earlier approach where the UI sampled with point filtering.
