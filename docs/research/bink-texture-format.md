# Bink surface formats, and what a texture backend needs from them

How Bink decides what to write, what it leaves out, and why that last part broke
full screen movies on the OpenGL backend for a week of investigation.

This is the pixel-format half of the movie story. The frame-by-frame account of
getting movies drawing at all - viewports, clip rects, where the draw is called
from - is in `graphics-backend-port.md` under "Full screen Bink movies".

## The chain

A movie frame reaches the screen through four hand-offs, and the format changes
meaning at each one.

```
  Bink codec
      |  BinkCopyToBuffer, format chosen by binkFlags
      v
  Bitmap pixels (bmpData)          A8R8G8B8, 0xAARRGGBB per U32
      |  Bitmap::UploadBackendTexture
      v
  GL texture object                GL_RGBA8
      |  fragment shader, texOp
      v
  framebuffer
```

DirectX only ever used the first two. The surface was blitted, and a blit copies
bytes - it never samples, never filters, and never looks at alpha. Everything
below the second arrow is new, and every assumption the first two steps were
allowed to make had to be re-examined.

## What Bink is asked for

`Bitmap::BinkSetFlags` picks the surface constant from the bitmap's own pixel
format, purely by bit count:

| Bitmap format | `binkFlags` | Value |
| --- | --- | --- |
| 32 bpp | `BINKSURFACE32` | 3 |
| 16 bpp, 5-5-5 | `BINKSURFACE555` | 9 |
| 16 bpp, 5-6-5 | `BINKSURFACE565` | 10 |
| 16 bpp, 6-5-5 | `BINKSURFACE655` | 11 |
| 16 bpp, 6-6-4 | `BINKSURFACE664` | 12 |

Two details matter.

**`BINKSURFACE32` is X8R8G8B8, not A8R8G8B8.** Bink writes three colour bytes
per pixel and leaves the fourth alone. `bink.h` does define an alpha-carrying
variant - `BINKSURFACE32A`, value 5 - and `BinkSetFlags` never selects it. There
is no branch that could: the choice is made on `dwRGBBitCount` alone, with no
regard for whether the format has an alpha mask.

**The DirectDraw-native path is dead code.** Sitting under an `#if 0` in the same
function is `binkFlags = BinkDDSurfaceType(surface)`, which would have asked Bink
to identify the surface itself. It has been disabled long enough that nobody
remembers; the bit-count mapping is what runs.

The movie bitmap's format comes from `MoviePlayer::Start`:

```cpp
bitmap->SetPixFmt(Vid::backBmp.PixelFormat());
```

and on the OpenGL path `InitFormatsOGL` registers exactly one format, used for
both textures and the back buffer:

```
R 0x00ff0000   G 0x0000ff00   B 0x000000ff   A 0xff000000
```

32 bpp, so `BINKSURFACE32`, so no alpha. Every full screen movie frame arrives
with all 307,200 pixels fully transparent.

## Why that was fatal

`Bitmap::Create` zeroes the buffer it allocates, and Bink never touches the top
byte, so alpha stays 0 for the life of the movie.

Under DirectDraw this was invisible and harmless. `Bitmap::Manager::MovieNextFrame`
blits the surface into the back buffer, and a blit does not blend.

As a texture it is fatal, and not for the reason you would first assume. The
draw already uses an opaque blend - `RS_SRC_ONE | RS_DST_ZERO` - so framebuffer
blending cannot be what discards it, and the shader's modulate path computes
`c.rgb = t.rgb * vDiffuse.rgb` with a white vertex colour, so the alpha is not
multiplied into the colour there either. By both of those the frame should be
visible.

It is not. **This driver stores texture data premultiplied**, so a `GL_RGBA8`
texture uploaded with zero alpha comes back with its colour zeroed. The upload
succeeds, `glGetError` is clean, the texture reports 640x480 `GL_RGBA8`, and
`glGetTexImage` reads back solid black.

That is measured, not inferred. With the alpha fill in `Bitmap::BinkDoFrame` the
intro plays; with the single condition flipped to `FALSE` and nothing else
changed, the screen is black. Both captured, twice, at the same point in the
movie.

The fix is three lines in `BinkDoFrame`, on the OpenGL path only:

```cpp
row[xx] |= 0xFF000000;
```

An opaque video frame should say it is opaque. Doing it here rather than in the
uploader keeps it next to the reason - it is Bink's output that is short an
alpha channel, not the texture path that is wrong.

### Alternatives, and why not

**Ask Bink for `BINKSURFACE32A`.** The cleanest answer in principle, and free -
Bink would write the alpha itself. Rejected because `BinkSetFlags` is engine
wide: it serves movie textures and the back buffer as well as full screen
playback, on both backends. Changing what every Bink surface in the game
requests, to fix one path on one backend, is a much larger blast radius than a
loop over one bitmap.

**Upload as `GL_RGB8` and ignore the fourth byte.** Would work, but only for
this one texture, and it would need `TextureUpload` to grow a per-bitmap notion
of which formats carry alpha - pushing a Bink quirk down into a function that
has no business knowing about it.

**Swizzle alpha to one in the shader.** Needs a uniform and a branch on the hot
path for every fragment in the game, to correct one texture.

## The second format trap: mip completeness

Worth recording alongside, because it presents identically - a texture that
uploads without error and samples black - and it is not specific to movies.

The engine sets filter state as though it were global device state, because in
Direct3D 7 it is. In GL it is per texture object, so `BeginDraw` applies whatever
was last asked for to the texture actually being drawn. With mipmapping enabled -
the default - that is `GL_LINEAR_MIPMAP_LINEAR`.

The movie texture is created with `mips = 0`. Sampling a texture through a
mipmap minification filter when it has no mip levels returns black here.

`TextureUpload` already did the right thing for a texture it had just built,
setting `GL_TEXTURE_MAX_LEVEL` to 0 and a non-mipmap filter. `BeginDraw` then
overwrote the filter on every single draw. It now downgrades to the mipmap-free
equivalent for any texture reporting no mip levels:

```cpp
tex->GetMipCount() ? wantMinFilter : NoMipFilter(wantMinFilter)
```

Nothing else in the interface had shown this, because everything else either
carries mip levels or happens to be drawn while the engine has mipmapping off.
The movie is drawn from `Vid::RenderFlush`, outside the interface's own filter
setup, so it inherits whatever the last thing to draw happened to leave behind.

## Wiring Bink to a bitmap with no surface

`BinkDoFrame` does not write through `bmpData`. It writes through the DirectDraw
surface description:

```cpp
BinkCopyToBuffer(bink, desc.lpSurface, desc.lPitch, desc.dwHeight, x, y, binkFlags);
```

On the DirectX path `Lock()` fills `desc` from the surface. There is no surface
here, so `Bitmap::Create` points the description at the pixels it just allocated,
and overrides the dimensions as well:

```cpp
desc.dwWidth  = bmpWidth;
desc.dwHeight = bmpHeight;
desc.lPitch   = bmpPitch;
desc.lpSurface = bmpData;
```

The dimensions matter because of how `BinkDoFrame` positions the frame:

```cpp
S32 x = (desc.dwWidth - bink->Width) >> 1;
S32 y = (desc.dwHeight - bink->Height) >> 2;
```

`desc.dwWidth` was rounded up to a dword boundary for DirectDraw's benefit. Left
rounded, a movie whose width is not a multiple of four would be written at a
positive `x` into a buffer allocated with no room for the offset. Forcing the
description to the bitmap's real size makes both terms zero.

That `>> 2` on the vertical is not a typo of mine - it is in the original, and it
means a frame smaller than its surface is placed a quarter of the way down
rather than centred. It has no effect while the surface is exactly frame sized,
which it now always is on this path, but it is worth knowing about before anyone
gives a movie bitmap a larger surface than its movie.

## Checklist for the next format

If another decoder or image source is added to this backend, these are the
questions this one answered the hard way:

1. Does it write alpha, or only colour? If only colour, the buffer needs alpha
   supplied - a blit-era source has never had to care.
2. Does it write through the pixel pointer, or through a surface description
   that a lock used to populate?
3. Does it produce mip levels? If not, the filter has to be downgraded at draw
   time, because the engine's filter state is global and will not be.
4. Are the buffer dimensions the real ones, or padded for a hardware alignment
   that no longer applies?

## Where this lives

| | |
| --- | --- |
| Format selection | `Bitmap::BinkSetFlags`, `graphics/bitmap.cpp` |
| Decode, and the alpha fill | `Bitmap::BinkDoFrame`, `graphics/bitmap.cpp` |
| Surface description wiring | `Bitmap::Create`, OpenGL branch, `graphics/bitmap.cpp` |
| Upload | `TextureUpload`, `graphics/vid_backend_ogl.cpp` |
| Filter downgrade | `BeginDraw`, `graphics/vid_backend_ogl.cpp` |
| Draw | `Bitmap::Manager::RenderExclusive`, `graphics/bitmap_manager.cpp` |
| Surface constants | `3rdparty/bink/bink.h` |
