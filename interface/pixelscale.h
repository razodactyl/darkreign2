///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Pixel Art Scaling System
//
// Provides high-quality scaling for pixel art (fonts, UI textures) that
// maintains crisp edges and avoids blurring or sub-pixel artifacts.
//
// Implements algorithms from: https://en.wikipedia.org/wiki/Pixel-art_scaling_algorithms
//

#ifndef __PIXELSCALE_H
#define __PIXELSCALE_H

#include "utiltypes.h"

// graphics/bitmap.h. Declared here at global scope on purpose: writing
// `class Bitmap*` in the ScaleBitmapUI declaration below would introduce
// PixelScale::Bitmap in any translation unit that had not already included
// bitmap.h, and callers would then fail to match with an unrelated-types
// error that points nowhere useful.
class Bitmap;

///////////////////////////////////////////////////////////////////////////////
//
// NameSpace PixelScale
//
namespace PixelScale
{
    //
    // Scaling algorithms available
    //
    enum Algorithm
    {
        // Nearest neighbor - fastest, maintains hard pixel edges
        // Best for: small integer scale factors (2x, 3x, 4x)
        NEAREST,

        // EPX/Scale2x/AdvMAME2x - edge-smoothing algorithm
        // Doubles size while smoothing diagonal edges
        // Algorithm by Eric Johnston (LucasArts, 1992)
        //
        //   A      1 2
        // C P B -> 3 4
        //   D
        //
        // 1=P; 2=P; 3=P; 4=P;
        // IF C==A AND C!=D AND A!=B => 1=A
        // IF A==B AND A!=C AND B!=D => 2=B
        // IF D==C AND D!=B AND C!=A => 3=C
        // IF B==D AND B!=A AND D!=C => 4=D
        SCALE2X,

        // Scale3x/AdvMAME3x - edge-smoothing algorithm  
        // Triples size while smoothing diagonal edges
        //
        // A B C    1 2 3
        // D E F -> 4 5 6
        // G H I    7 8 9
        SCALE3X,

        // Eagle - simple 2x scaling with corner smoothing
        // Sets corners to neighbor color if 3 neighbors match
        EAGLE,

        // hq2x - 2x magnification with YUV edge detection and interpolation
        //
        // NOTE: this is an APPROXIMATION of Maxim Stepin's hq2x, not the real
        // thing. Genuine hqx switches on a 256-entry table of neighbour
        // patterns with hand-tuned rules per case; this applies four
        // symmetric corner rules instead. It anti-aliases diagonals, but it
        // will round off corners that real hq2x preserves. See
        // docs/research/pixel-scaling.md.
        HQ2X,

        // hq3x - 3x variant of the above, same caveat
        HQ3X,

        // Number of algorithms; must stay last
        ALGORITHM_COUNT,

        // Default for everything. Nearest neighbour is the only algorithm that
        // is unconditionally safe on this content: the game's art is already
        // anti-aliased continuous tone, and the pixel-art filters below assume
        // hard-edged indexed source. NN also preserves the property the whole
        // system exists for - texels landing exactly on pixel boundaries.
        DEFAULT = NEAREST,

        // Recommended for fonts/text (preserves hard edges, no new colors)
        DEFAULT_TEXT = NEAREST
    };

    //
    // Master switch (--upscale). When off, GetIntegerScale() reports 1,
    // IFace::GetScale() reports 1.0, and nothing is pre-scaled: the UI is laid
    // out and drawn exactly as it was before the high-resolution work.
    //
    void SetEnabled(Bool on);
    Bool GetEnabled();

    //
    // Layout scaling (--ui4k). Controls IFace::GetScale() only: whether
    // control geometry loaded in 640x480 design space is scaled up to the
    // current resolution. Independent of texture pre-scaling.
    //
    void SetUIScaling(Bool on);
    Bool GetUIScaling();

    //
    // Texture pre-scaling (--texup). Controls whether UI textures are built at
    // an integer multiple of their native size. Independent of layout scaling.
    //
    void SetTextureScaling(Bool on);
    Bool GetTextureScaling();

    //
    // Font atlas pre-scaling (--fontup), and the algorithm it uses.
    //
    // Fonts are held separately from textures because they are the one thing
    // being pre-scaled today, and because their content is unlike the rest:
    // white glyphs carrying all their shape in an 8 bit alpha ramp. The
    // pixel-art filters read that ramp as a stack of diagonals, so the font
    // default stays NEAREST no matter what --texup is set to.
    //
    void SetFontScaling(Bool on);
    Bool GetFontScaling();

    void SetFontAlgorithm(Algorithm algo);
    Algorithm GetFontAlgorithm();

    //
    // Algorithm names for the command line: "nn", "nearest", "scale2x",
    // "scale3x", "eagle", "hq2x", "hq3x". Returns FALSE if unrecognised.
    //
    Bool ParseAlgorithm(const char* name, Algorithm& algo);
    const char* AlgorithmName(Algorithm algo);

    //
    // The scale factor an algorithm natively produces (2 or 3; 1 for NEAREST,
    // which works at any factor)
    //
    S32 NativeFactor(Algorithm algo);

    //
    // Get the recommended integer scale factor for current resolution
    // Returns floor(IFace::GetRawScale()) clamped to [1, maxScale], or 1 when
    // scaling is switched off altogether
    //
    S32 GetIntegerScale(S32 maxScale = 4);

    //
    // The same figure for a particular consumer, reporting 1 when that
    // consumer's own switch is off. Use these rather than GetIntegerScale
    // when deciding how large to build something.
    //
    S32 GetTextureScale(S32 maxScale = 4);
    S32 GetFontScale(S32 maxScale = 4);

    //
    // Get the fractional remainder after integer scaling
    // e.g., if raw scale is 2.5 and integer scale is 2, returns 0.25 (the 25% extra)
    // This can be used to slightly adjust positions to account for the remainder
    //
    F32 GetScaleRemainder();

    //
    // Check if we should use integer scaling (recommended for pixel art)
    // Returns true if the scale factor is close to an integer (within tolerance)
    //
    Bool ShouldUseIntegerScale(F32 tolerance = 0.1f);

    //
    // Scale a dimension using integer scaling
    // Multiplies by floor(scale) to maintain pixel-perfect sizing
    //
    S32 ScaleDimension(S32 value);

    //
    // Scale a position using integer scaling
    // For positions, we may want to add sub-pixel offset for centering
    //
    S32 ScalePosition(S32 value, Bool centerRemainder = FALSE);

    //
    // Scale a floating point value using integer scaling
    //
    F32 ScaleF(F32 value);

    //
    // Unscale a screen-space value back to design-space
    //
    S32 Unscale(S32 screenValue);
    F32 UnscaleF(F32 screenValue);

    //
    // Get UV coordinates for a scaled texture region
    // When rendering a texture at integer scale, we may need to adjust UVs
    // to avoid sampling artifacts at texel boundaries
    //
    void GetScaledUV(F32 srcU, F32 srcV, F32 srcW, F32 srcH,
                     F32& outU, F32& outV, F32& outW, F32& outH,
                     S32 texWidth, S32 texHeight);

    //
    // Calculate the pixel-perfect destination rectangle for a source rectangle
    // Ensures the destination aligns to pixel boundaries
    //
    void GetPixelPerfectRect(S32 srcX, S32 srcY, S32 srcW, S32 srcH,
                             S32& dstX, S32& dstY, S32& dstW, S32& dstH);

    //
    // Initialize the pixel scaling system
    // Called when resolution changes
    //
    void Init();

    //
    // Set the preferred scaling algorithm
    //
    void SetAlgorithm(Algorithm algo);
    Algorithm GetAlgorithm();

    //
    // Configuration
    //
    void SetMaxScale(S32 max);
    S32 GetMaxScale();

    //
    // Scale2x algorithm (EPX/AdvMAME2x)
    // Scales a source image to 2x size with edge smoothing
    // 
    // src: Source pixel data (32-bit ARGB)
    // srcWidth, srcHeight: Source dimensions
    // dst: Destination buffer (must be 2x width and 2x height)
    //
    void Scale2x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst);

    //
    // Scale3x algorithm (AdvMAME3x)
    // Scales a source image to 3x size with edge smoothing
    //
    void Scale3x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst);

    //
    // Eagle algorithm
    // Simple 2x scaling with corner smoothing
    //
    void Eagle2x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst);

    //
    // hq2x algorithm (Maxim Stepin)
    // High quality 2x scaling with YUV color comparison and interpolation
    //
    void Hq2x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst);

    //
    // hq3x algorithm (Maxim Stepin)
    // High quality 3x scaling with YUV color comparison and interpolation
    //
    void Hq3x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst);

    //
    // Nearest neighbour at an arbitrary integer factor
    //
    void Nearest(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst, S32 factor);

    //
    // Generic scale function - uses current algorithm setting
    // Returns the scale factor used (2 or 3 depending on algorithm)
    // Caller must ensure dst buffer is large enough
    //
    S32 ScaleImage(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst);

    //
    // Scale by an exact factor using a given algorithm, falling back within
    // the algorithm's family when it has no native variant for that factor
    // (e.g. Scale2x at factor 3 uses Scale3x; anything with no equivalent
    // uses nearest neighbour, which never distorts).
    //
    // dst must hold srcWidth * factor by srcHeight * factor pixels.
    //
    void ScaleImageWith(Algorithm algo, const U32* src, S32 srcWidth, S32 srcHeight,
                        U32* dst, S32 factor);

    //
    // ScaleImageWith using the current general algorithm
    //
    void ScaleImageTo(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst, S32 factor);

    //
    // Get required destination buffer size for scaling
    //
    S32 GetScaledWidth(S32 srcWidth);
    S32 GetScaledHeight(S32 srcHeight);

    //
    // Pre-scale an interface texture in place, using the current general
    // algorithm and the factor from GetTextureScale. Returns the factor now
    // applied, 1 when nothing was done.
    //
    // Does nothing unless texture scaling is switched on (-texup). The
    // factor is recorded on the bitmap - Bitmap::UIScale - so it is reset
    // by whatever replaces the pixels and cannot be left stale; callers
    // needing it later should ask the bitmap rather than remember it.
    //
    S32 ScaleBitmapUI(Bitmap* bmp);
}

#endif // __PIXELSCALE_H
