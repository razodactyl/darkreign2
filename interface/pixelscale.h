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

///////////////////////////////////////////////////////////////////////////////
//
// Experimental feature flags
//
// Define PIXELSCALE_SCALE2X_UI to enable Scale2x scaling for UI textures
// Scale2x preserves hard edges (good for titlebars, borders, buttons)
// Comment out to use standard GPU bilinear scaling
//
// NOTE: DISABLED - causes texture corruption when bitmaps are reloaded.
//       The bitmap tracking doesn't handle bitmap recreation properly.
//       Font pre-scaling is handled separately in font.cpp and works correctly.
// #define PIXELSCALE_SCALE2X_UI
//

#include "utiltypes.h"

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

        // hq2x - high quality 2x magnification filter
        // Uses YUV color comparison and lookup tables for smooth anti-aliased output
        // Best quality but slower than Scale2x
        HQ2X,

        // hq3x - high quality 3x magnification filter
        HQ3X,

        // Default algorithm for general images (introduces anti-aliasing)
        DEFAULT = HQ2X,

        // Recommended for fonts/text (preserves hard edges, no new colors)
        DEFAULT_TEXT = SCALE2X
    };

    //
    // Get the recommended integer scale factor for current resolution
    // Returns floor(IFace::GetScale()) clamped to [1, maxScale]
    //
    S32 GetIntegerScale(S32 maxScale = 4);

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
    // Generic scale function - uses current algorithm setting
    // Returns the scale factor used (2 or 3 depending on algorithm)
    // Caller must ensure dst buffer is large enough
    //
    S32 ScaleImage(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst);

    //
    // Get required destination buffer size for scaling
    //
    S32 GetScaledWidth(S32 srcWidth);
    S32 GetScaledHeight(S32 srcHeight);

#ifdef PIXELSCALE_SCALE2X_UI
    //
    // Scale a bitmap's pixel data using Scale2x
    // Properly handles the bitmap's native pixel format
    // Returns the scale factor applied (2 if scaled, 1 if not)
    //
    // Note: Tracks scaled bitmaps to avoid double-scaling cached textures
    //
    S32 ScaleBitmapUI(class Bitmap* bmp);

    //
    // Clear the scaled bitmap tracking (call on resolution change)
    //
    void ClearScaledBitmaps();
#endif
}

#endif // __PIXELSCALE_H
