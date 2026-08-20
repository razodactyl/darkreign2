///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Pixel Art Scaling System
//
// Implements pixel art scaling algorithms:
// - Scale2x/EPX/AdvMAME2x (Eric Johnston, LucasArts 1992)
// - Scale3x/AdvMAME3x
// - Eagle
//
// Reference: https://en.wikipedia.org/wiki/Pixel-art_scaling_algorithms
//

#include "pixelscale.h"
#include "iface.h"
#include "bitmap.h"
#include "vid_public.h"
#include <cmath>

namespace PixelScale
{
    //
    // Module state
    //
    static Algorithm currentAlgorithm = DEFAULT;
    static S32 maxScaleFactor = 4;
    static S32 cachedIntegerScale = 1;
    static F32 cachedRemainder = 0.0f;
    static Bool initialized = FALSE;

    // Set from the command line before anything renders; see
    // Main::ProcessCommandLine
    static Bool enabled = TRUE;
    static Bool uiScaling = TRUE;
    // Texture pre-scaling is off unless asked for. The filters are
    // selectable so they can be evaluated on real interface art, but the
    // default has to leave the shipping game exactly as it was.
    static Bool textureScaling = FALSE;
    static Bool fontScaling = TRUE;

    // Fonts keep their own algorithm and default to NEAREST regardless of
    // what --texup selects; see the note in pixelscale.h
    static Algorithm fontAlgorithm = NEAREST;

    //
    // Algorithm names as they appear on the command line. Indexed by
    // Algorithm, so the order must track the enum.
    //
    static const char* algorithmNames[ALGORITHM_COUNT] =
    {
        "nn",
        "scale2x",
        "scale3x",
        "eagle",
        "hq2x",
        "hq3x",
    };

    //
    // Helper: Get pixel with bounds checking (clamp to edge)
    //
    static inline U32 GetPixel(const U32* src, S32 width, S32 height, S32 x, S32 y)
    {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= width) x = width - 1;
        if (y >= height) y = height - 1;
        return src[y * width + x];
    }

    //
    // Helper: Set pixel in destination
    //
    static inline void SetPixel(U32* dst, S32 dstWidth, S32 x, S32 y, U32 color)
    {
        dst[y * dstWidth + x] = color;
    }

    //
    // Helper: Extract color components
    //
    static inline void GetRGBA(U32 color, S32& r, S32& g, S32& b, S32& a)
    {
        a = (color >> 24) & 0xFF;
        r = (color >> 16) & 0xFF;
        g = (color >> 8) & 0xFF;
        b = color & 0xFF;
    }

    //
    // Helper: Make color from components
    //
    static inline U32 MakeRGBA(S32 r, S32 g, S32 b, S32 a)
    {
        return (U32(a & 0xFF) << 24) | (U32(r & 0xFF) << 16) | (U32(g & 0xFF) << 8) | U32(b & 0xFF);
    }

    //
    // Helper: Convert RGB to YUV for perceptual color comparison
    // Y = luminance, U/V = chrominance
    //
    static inline void RGBtoYUV(S32 r, S32 g, S32 b, S32& y, S32& u, S32& v)
    {
        y = (r + g + b) / 3;  // Simplified luminance
        u = 128 + (r - b) / 2;
        v = 128 + (g * 2 - r - b) / 4;
    }

    //
    // Helper: Check if two colors are "different" using YUV comparison
    // This is the key to hq2x - it determines edge detection
    //
    static inline Bool ColorsDifferent(U32 c1, U32 c2, S32 threshold = 48)
    {
        if (c1 == c2) return FALSE;

        S32 r1, g1, b1, a1, r2, g2, b2, a2;
        GetRGBA(c1, r1, g1, b1, a1);
        GetRGBA(c2, r2, g2, b2, a2);

        // Alpha difference check
        if (abs(a1 - a2) > threshold) return TRUE;

        // YUV comparison for perceptual similarity
        S32 y1, u1, v1, y2, u2, v2;
        RGBtoYUV(r1, g1, b1, y1, u1, v1);
        RGBtoYUV(r2, g2, b2, y2, u2, v2);

        return (abs(y1 - y2) > threshold) || 
               (abs(u1 - u2) > (threshold / 4)) || 
               (abs(v1 - v2) > (threshold / 4));
    }

    //
    // Helper: weighted blend of up to four colors.
    //
    // Colour is averaged in premultiplied-alpha space and un-premultiplied on
    // the way out. Averaging the raw channels instead - which is what this
    // used to do - drags the colour of transparent texels into their opaque
    // neighbours. Most of the game's UI art stores black under its
    // transparent pixels, so a straight average draws a dark fringe around
    // every edge it touches. Font glyphs happen to be white everywhere and so
    // could not show the problem, which is presumably why it survived.
    //
    static inline U32 Blend(const U32* c, const S32* w, S32 count)
    {
        S32 rSum = 0, gSum = 0, bSum = 0, aSum = 0, wSum = 0;

        for (S32 i = 0; i < count; i++)
        {
            S32 r, g, b, a;
            GetRGBA(c[i], r, g, b, a);

            // premultiply, so a transparent texel contributes no colour
            rSum += r * a * w[i];
            gSum += g * a * w[i];
            bSum += b * a * w[i];
            aSum += a * w[i];
            wSum += w[i];
        }

        if (aSum == 0)
        {
            // every contributor was fully transparent - there is no colour to
            // recover, and the result is invisible either way
            return 0;
        }

        // un-premultiply: the alpha weighting cancels out of the divisor
        return MakeRGBA(rSum / aSum, gSum / aSum, bSum / aSum, aSum / wSum);
    }

    //
    // Helper: Interpolate two colors (50/50 blend)
    //
    static inline U32 Interp2(U32 c1, U32 c2)
    {
        const U32 c[2] = { c1, c2 };
        const S32 w[2] = { 1, 1 };
        return Blend(c, w, 2);
    }

    //
    // Helper: Interpolate three colors (2:1:1 ratio)
    //
    static inline U32 Interp3(U32 c1, U32 c2, U32 c3)
    {
        const U32 c[3] = { c1, c2, c3 };
        const S32 w[3] = { 2, 1, 1 };
        return Blend(c, w, 3);
    }

    //
    // Helper: Interpolate four colors (equal weight)
    //
    static inline U32 Interp4(U32 c1, U32 c2, U32 c3, U32 c4)
    {
        const U32 c[4] = { c1, c2, c3, c4 };
        const S32 w[4] = { 1, 1, 1, 1 };
        return Blend(c, w, 4);
    }

    //
    // Initialize / update cached values
    //
    void Init()
    {
        // Deliberately the *raw* scale, not IFace::GetScale(): that one is
        // gated on the --ui4k toggle, and pre-scaling is meant to be
        // independent of whether control geometry is being scaled.
        //
        // Only the master switch is folded in here. The per-consumer switches
        // are applied by GetTextureScale and GetFontScale, so that turning one
        // off cannot affect the other.
        F32 rawScale = enabled ? IFace::GetRawScale() : 1.0f;

        // Calculate integer scale (floor, clamped to [1, maxScale])
        cachedIntegerScale = S32(floorf(rawScale));
        if (cachedIntegerScale < 1) cachedIntegerScale = 1;
        if (cachedIntegerScale > maxScaleFactor) cachedIntegerScale = maxScaleFactor;
        
        // Calculate remainder for sub-pixel positioning
        cachedRemainder = rawScale - F32(cachedIntegerScale);
        
        initialized = TRUE;
    }

    //
    // Ensure initialized
    //
    static void EnsureInit()
    {
        if (!initialized)
        {
            Init();
        }
    }

    //
    // Get integer scale factor
    //
    S32 GetIntegerScale(S32 maxScale)
    {
        EnsureInit();
        
        S32 scale = cachedIntegerScale;
        if (scale > maxScale) scale = maxScale;
        return scale;
    }

    //
    // Per-consumer scale factors
    //
    S32 GetTextureScale(S32 maxScale)
    {
        return GetTextureScaling() ? GetIntegerScale(maxScale) : 1;
    }

    S32 GetFontScale(S32 maxScale)
    {
        return GetFontScaling() ? GetIntegerScale(maxScale) : 1;
    }

    //
    // Get the fractional remainder
    //
    F32 GetScaleRemainder()
    {
        EnsureInit();
        return cachedRemainder;
    }

    //
    // Check if integer scaling is appropriate
    //
    Bool ShouldUseIntegerScale(F32 tolerance)
    {
        EnsureInit();
        return cachedRemainder < tolerance;
    }

    //
    // Scale a dimension
    //
    S32 ScaleDimension(S32 value)
    {
        EnsureInit();
        return value * cachedIntegerScale;
    }

    //
    // Scale a position
    //
    S32 ScalePosition(S32 value, Bool centerRemainder)
    {
        EnsureInit();
        
        S32 scaled = value * cachedIntegerScale;
        
        if (centerRemainder && cachedRemainder > 0.0f)
        {
            // Add half the remainder to center the content
            // This helps when the actual scale is between integers
            scaled += S32(F32(value) * cachedRemainder * 0.5f);
        }
        
        return scaled;
    }

    //
    // Scale a float value
    //
    F32 ScaleF(F32 value)
    {
        EnsureInit();
        return value * F32(cachedIntegerScale);
    }

    //
    // Unscale back to design space
    //
    S32 Unscale(S32 screenValue)
    {
        EnsureInit();
        if (cachedIntegerScale == 0) return screenValue;
        return screenValue / cachedIntegerScale;
    }

    F32 UnscaleF(F32 screenValue)
    {
        EnsureInit();
        if (cachedIntegerScale == 0) return screenValue;
        return screenValue / F32(cachedIntegerScale);
    }

    //
    // Get scaled UV coordinates with half-texel offset to avoid sampling artifacts
    //
    void GetScaledUV(F32 srcU, F32 srcV, F32 srcW, F32 srcH,
                     F32& outU, F32& outV, F32& outW, F32& outH,
                     S32 texWidth, S32 texHeight)
    {
        // Half-texel offset to sample from texel centers
        // This prevents bleeding from adjacent texels when using point filtering
        F32 halfTexelU = 0.5f / F32(texWidth);
        F32 halfTexelV = 0.5f / F32(texHeight);
        
        outU = srcU + halfTexelU;
        outV = srcV + halfTexelV;
        outW = srcW - halfTexelU * 2.0f;
        outH = srcH - halfTexelV * 2.0f;
    }

    //
    // Calculate pixel-perfect destination rectangle
    //
    void GetPixelPerfectRect(S32 srcX, S32 srcY, S32 srcW, S32 srcH,
                             S32& dstX, S32& dstY, S32& dstW, S32& dstH)
    {
        EnsureInit();
        
        // Scale dimensions by integer factor
        dstW = srcW * cachedIntegerScale;
        dstH = srcH * cachedIntegerScale;
        
        // Scale position by integer factor
        dstX = srcX * cachedIntegerScale;
        dstY = srcY * cachedIntegerScale;
    }

    //
    // Feature toggles. These are set from the command line before IFace comes
    // up, so they only need to re-run Init if it has already happened.
    //
    void SetEnabled(Bool on)
    {
        enabled = on;
        if (initialized)
        {
            Init();
        }
    }

    Bool GetEnabled()
    {
        return enabled;
    }

    void SetUIScaling(Bool on)
    {
        uiScaling = on;
    }

    Bool GetUIScaling()
    {
        // the master switch overrides the individual one
        return enabled && uiScaling;
    }

    void SetTextureScaling(Bool on)
    {
        textureScaling = on;
    }

    Bool GetTextureScaling()
    {
        return enabled && textureScaling;
    }

    void SetFontScaling(Bool on)
    {
        fontScaling = on;
    }

    Bool GetFontScaling()
    {
        return enabled && fontScaling;
    }

    void SetFontAlgorithm(Algorithm algo)
    {
        fontAlgorithm = algo;
    }

    Algorithm GetFontAlgorithm()
    {
        return fontAlgorithm;
    }

    //
    // Algorithm names
    //
    Bool ParseAlgorithm(const char* name, Algorithm& algo)
    {
        // "nearest" is accepted as a synonym for the canonical "nn"
        if (!Utils::Stricmp(name, "nearest"))
        {
            algo = NEAREST;
            return TRUE;
        }

        for (S32 i = 0; i < ALGORITHM_COUNT; i++)
        {
            if (!Utils::Stricmp(name, algorithmNames[i]))
            {
                algo = Algorithm(i);
                return TRUE;
            }
        }

        return FALSE;
    }

    const char* AlgorithmName(Algorithm algo)
    {
        if (algo < 0 || algo >= ALGORITHM_COUNT)
        {
            return "?";
        }
        return algorithmNames[algo];
    }

    //
    // Native output factor of an algorithm. NEAREST reports 1 because it is
    // exact at every factor rather than tied to one.
    //
    S32 NativeFactor(Algorithm algo)
    {
        switch (algo)
        {
            case SCALE3X:
            case HQ3X:
                return 3;

            case SCALE2X:
            case EAGLE:
            case HQ2X:
                return 2;

            case NEAREST:
            default:
                return 1;
        }
    }

    //
    // Set algorithm
    //
    void SetAlgorithm(Algorithm algo)
    {
        currentAlgorithm = algo;
    }

    Algorithm GetAlgorithm()
    {
        return currentAlgorithm;
    }

    //
    // Configuration
    //
    void SetMaxScale(S32 max)
    {
        maxScaleFactor = max;
        if (maxScaleFactor < 1) maxScaleFactor = 1;
        
        // Recalculate cached values
        if (initialized)
        {
            Init();
        }
    }

    S32 GetMaxScale()
    {
        return maxScaleFactor;
    }

    //
    // Scale2x / EPX / AdvMAME2x algorithm
    //
    // For each pixel P with neighbors:
    //     A
    //   C P B
    //     D
    //
    // Output 2x2 block:
    //   1 2
    //   3 4
    //
    // Rules:
    //   1=P; 2=P; 3=P; 4=P;
    //   IF C==A AND C!=D AND A!=B => 1=A
    //   IF A==B AND A!=C AND B!=D => 2=B
    //   IF D==C AND D!=B AND C!=A => 3=C
    //   IF B==D AND B!=A AND D!=C => 4=D
    //
    void Scale2x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst)
    {
        S32 dstWidth = srcWidth * 2;

        for (S32 y = 0; y < srcHeight; y++)
        {
            for (S32 x = 0; x < srcWidth; x++)
            {
                // Get center pixel and neighbors
                U32 P = GetPixel(src, srcWidth, srcHeight, x, y);
                U32 A = GetPixel(src, srcWidth, srcHeight, x, y - 1);  // Above
                U32 B = GetPixel(src, srcWidth, srcHeight, x + 1, y);  // Right
                U32 C = GetPixel(src, srcWidth, srcHeight, x - 1, y);  // Left
                U32 D = GetPixel(src, srcWidth, srcHeight, x, y + 1);  // Below

                // Default: all output pixels = center pixel
                U32 p1 = P, p2 = P, p3 = P, p4 = P;

                // Apply Scale2x rules
                if (C == A && C != D && A != B) p1 = A;
                if (A == B && A != C && B != D) p2 = B;
                if (D == C && D != B && C != A) p3 = C;
                if (B == D && B != A && D != C) p4 = D;

                // Write 2x2 output block
                S32 dstX = x * 2;
                S32 dstY = y * 2;
                SetPixel(dst, dstWidth, dstX, dstY, p1);
                SetPixel(dst, dstWidth, dstX + 1, dstY, p2);
                SetPixel(dst, dstWidth, dstX, dstY + 1, p3);
                SetPixel(dst, dstWidth, dstX + 1, dstY + 1, p4);
            }
        }
    }

    //
    // Scale3x / AdvMAME3x algorithm
    //
    // For each pixel E with neighbors:
    //   A B C
    //   D E F
    //   G H I
    //
    // Output 3x3 block:
    //   1 2 3
    //   4 5 6
    //   7 8 9
    //
    void Scale3x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst)
    {
        S32 dstWidth = srcWidth * 3;

        for (S32 y = 0; y < srcHeight; y++)
        {
            for (S32 x = 0; x < srcWidth; x++)
            {
                // Get center pixel and all 8 neighbors
                U32 A = GetPixel(src, srcWidth, srcHeight, x - 1, y - 1);
                U32 B = GetPixel(src, srcWidth, srcHeight, x, y - 1);
                U32 C = GetPixel(src, srcWidth, srcHeight, x + 1, y - 1);
                U32 D = GetPixel(src, srcWidth, srcHeight, x - 1, y);
                U32 E = GetPixel(src, srcWidth, srcHeight, x, y);
                U32 F = GetPixel(src, srcWidth, srcHeight, x + 1, y);
                U32 G = GetPixel(src, srcWidth, srcHeight, x - 1, y + 1);
                U32 H = GetPixel(src, srcWidth, srcHeight, x, y + 1);
                U32 I = GetPixel(src, srcWidth, srcHeight, x + 1, y + 1);

                // Default: all output pixels = center pixel
                U32 p1 = E, p2 = E, p3 = E;
                U32 p4 = E, p5 = E, p6 = E;
                U32 p7 = E, p8 = E, p9 = E;

                // Apply Scale3x rules
                if (D == B && D != H && B != F) p1 = D;
                if ((D == B && D != H && B != F && E != C) || (B == F && B != D && F != H && E != A)) p2 = B;
                if (B == F && B != D && F != H) p3 = F;
                if ((H == D && H != F && D != B && E != A) || (D == B && D != H && B != F && E != G)) p4 = D;
                // p5 = E (center always stays)
                if ((B == F && B != D && F != H && E != I) || (F == H && F != B && H != D && E != C)) p6 = F;
                if (H == D && H != F && D != B) p7 = D;
                if ((F == H && F != B && H != D && E != G) || (H == D && H != F && D != B && E != I)) p8 = H;
                if (F == H && F != B && H != D) p9 = F;

                // Write 3x3 output block
                S32 dstX = x * 3;
                S32 dstY = y * 3;
                SetPixel(dst, dstWidth, dstX, dstY, p1);
                SetPixel(dst, dstWidth, dstX + 1, dstY, p2);
                SetPixel(dst, dstWidth, dstX + 2, dstY, p3);
                SetPixel(dst, dstWidth, dstX, dstY + 1, p4);
                SetPixel(dst, dstWidth, dstX + 1, dstY + 1, p5);
                SetPixel(dst, dstWidth, dstX + 2, dstY + 1, p6);
                SetPixel(dst, dstWidth, dstX, dstY + 2, p7);
                SetPixel(dst, dstWidth, dstX + 1, dstY + 2, p8);
                SetPixel(dst, dstWidth, dstX + 2, dstY + 2, p9);
            }
        }
    }

    //
    // Eagle algorithm (simple 2x with corner smoothing)
    //
    // For each pixel C with neighbors:
    //   S T U
    //   V C W
    //   X Y Z
    //
    // Output 2x2 block:
    //   1 2
    //   3 4
    //
    // Rules:
    //   First set all to C, then:
    //   IF V==S==T => 1=S
    //   IF T==U==W => 2=U
    //   IF V==X==Y => 3=X
    //   IF W==Z==Y => 4=Z
    //
    void Eagle2x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst)
    {
        S32 dstWidth = srcWidth * 2;

        for (S32 y = 0; y < srcHeight; y++)
        {
            for (S32 x = 0; x < srcWidth; x++)
            {
                // Get center pixel and all 8 neighbors
                U32 S = GetPixel(src, srcWidth, srcHeight, x - 1, y - 1);
                U32 T = GetPixel(src, srcWidth, srcHeight, x, y - 1);
                U32 U = GetPixel(src, srcWidth, srcHeight, x + 1, y - 1);
                U32 V = GetPixel(src, srcWidth, srcHeight, x - 1, y);
                U32 C = GetPixel(src, srcWidth, srcHeight, x, y);
                U32 W = GetPixel(src, srcWidth, srcHeight, x + 1, y);
                U32 X = GetPixel(src, srcWidth, srcHeight, x - 1, y + 1);
                U32 Y = GetPixel(src, srcWidth, srcHeight, x, y + 1);
                U32 Z = GetPixel(src, srcWidth, srcHeight, x + 1, y + 1);

                // Default: all output pixels = center pixel
                U32 p1 = C, p2 = C, p3 = C, p4 = C;

                // Apply Eagle rules
                if (V == S && S == T) p1 = S;
                if (T == U && U == W) p2 = U;
                if (V == X && X == Y) p3 = X;
                if (W == Z && Z == Y) p4 = Z;

                // Write 2x2 output block
                S32 dstX = x * 2;
                S32 dstY = y * 2;
                SetPixel(dst, dstWidth, dstX, dstY, p1);
                SetPixel(dst, dstWidth, dstX + 1, dstY, p2);
                SetPixel(dst, dstWidth, dstX, dstY + 1, p3);
                SetPixel(dst, dstWidth, dstX + 1, dstY + 1, p4);
            }
        }
    }

    //
    // hq2x algorithm (simplified implementation based on Maxim Stepin's algorithm)
    //
    // Uses YUV color comparison to detect edges and interpolates colors
    // for smooth anti-aliased output. This produces higher quality than
    // Scale2x but introduces new colors (anti-aliasing).
    //
    // For each pixel E with neighbors:
    //   A B C
    //   D E F
    //   G H I
    //
    // Output 2x2 block with interpolated colors based on edge detection
    //
    void Hq2x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst)
    {
        S32 dstWidth = srcWidth * 2;

        for (S32 y = 0; y < srcHeight; y++)
        {
            for (S32 x = 0; x < srcWidth; x++)
            {
                // Get center pixel and all 8 neighbors
                U32 A = GetPixel(src, srcWidth, srcHeight, x - 1, y - 1);
                U32 B = GetPixel(src, srcWidth, srcHeight, x, y - 1);
                U32 C = GetPixel(src, srcWidth, srcHeight, x + 1, y - 1);
                U32 D = GetPixel(src, srcWidth, srcHeight, x - 1, y);
                U32 E = GetPixel(src, srcWidth, srcHeight, x, y);
                U32 F = GetPixel(src, srcWidth, srcHeight, x + 1, y);
                U32 G = GetPixel(src, srcWidth, srcHeight, x - 1, y + 1);
                U32 H = GetPixel(src, srcWidth, srcHeight, x, y + 1);
                U32 I = GetPixel(src, srcWidth, srcHeight, x + 1, y + 1);

                // Default output is center pixel
                U32 p1 = E, p2 = E, p3 = E, p4 = E;

                // A corner is only rewritten where the two edge neighbours
                // that meet at it agree with each other, disagree with the
                // centre, and the diagonal beyond the corner also disagrees -
                // that is, where a real diagonal edge is cutting the corner
                // off. This is Scale2x's test with a perceptual (YUV)
                // comparison instead of an exact one, and a blend instead of a
                // copy.
                //
                // There used to be an unguarded `else if (!ColorsDifferent(D,
                // B)) p1 = Interp3(E, D, B)` on each corner. That fired
                // whenever the two edge neighbours merely agreed, including
                // when the diagonal matched the centre - i.e. on convex
                // corners, where the correct output is the centre pixel
                // untouched. It softened every corner in the image by 50%,
                // which is the opposite of what a pixel-art filter is for.
                //
                // An `U32 pattern` bitmask of the eight neighbour comparisons
                // was also being built here and then never read. Real hqx
                // switches on that mask over 256 hand-tuned cases; this does
                // not, so the mask was dead weight and a misleading hint that
                // it did.
                if (!ColorsDifferent(D, B) && ColorsDifferent(E, D) && ColorsDifferent(E, A))
                {
                    p1 = Interp2(D, B);
                }

                if (!ColorsDifferent(B, F) && ColorsDifferent(E, B) && ColorsDifferent(E, C))
                {
                    p2 = Interp2(B, F);
                }

                if (!ColorsDifferent(D, H) && ColorsDifferent(E, D) && ColorsDifferent(E, G))
                {
                    p3 = Interp2(D, H);
                }

                if (!ColorsDifferent(H, F) && ColorsDifferent(E, H) && ColorsDifferent(E, I))
                {
                    p4 = Interp2(H, F);
                }

                // Write 2x2 output block
                S32 dstX = x * 2;
                S32 dstY = y * 2;
                SetPixel(dst, dstWidth, dstX, dstY, p1);
                SetPixel(dst, dstWidth, dstX + 1, dstY, p2);
                SetPixel(dst, dstWidth, dstX, dstY + 1, p3);
                SetPixel(dst, dstWidth, dstX + 1, dstY + 1, p4);
            }
        }
    }

    //
    // hq3x algorithm (simplified implementation)
    //
    // Similar to hq2x but outputs 3x3 block per input pixel
    //
    void Hq3x(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst)
    {
        S32 dstWidth = srcWidth * 3;

        for (S32 y = 0; y < srcHeight; y++)
        {
            for (S32 x = 0; x < srcWidth; x++)
            {
                // Get center pixel and all 8 neighbors
                U32 A = GetPixel(src, srcWidth, srcHeight, x - 1, y - 1);
                U32 B = GetPixel(src, srcWidth, srcHeight, x, y - 1);
                U32 C = GetPixel(src, srcWidth, srcHeight, x + 1, y - 1);
                U32 D = GetPixel(src, srcWidth, srcHeight, x - 1, y);
                U32 E = GetPixel(src, srcWidth, srcHeight, x, y);
                U32 F = GetPixel(src, srcWidth, srcHeight, x + 1, y);
                U32 G = GetPixel(src, srcWidth, srcHeight, x - 1, y + 1);
                U32 H = GetPixel(src, srcWidth, srcHeight, x, y + 1);
                U32 I = GetPixel(src, srcWidth, srcHeight, x + 1, y + 1);

                // Default: all output pixels = center pixel
                U32 p1 = E, p2 = E, p3 = E;
                U32 p4 = E, p5 = E, p6 = E;
                U32 p7 = E, p8 = E, p9 = E;

                // Same corner test as Hq2x above, and the same two corrections:
                // the convex-corner softening `else if` branches are gone, and
                // so is the edge blending that hung off them.
                //
                // The edge pixels used to be written like
                //
                //   if (!ColorsDifferent(B, B))          // <- B against itself
                //     p2 = (...) ? Interp2(E, B) : E;
                //
                // which compares a pixel with itself and is therefore always
                // taken; the comment even said so. Worse than the dead test is
                // what it guarded: p2 was blended halfway towards B whenever B
                // merely agreed with D or F, so every straight horizontal run
                // in the image had its top third blended into the row above
                // it. Straight edges must survive magnification untouched -
                // only diagonals get rewritten - so the four edge pixels now
                // stay as E, matching Scale3x.

                // Top-left corner
                if (!ColorsDifferent(D, B) && ColorsDifferent(E, D) && ColorsDifferent(E, A))
                    p1 = Interp2(D, B);

                // Top-right corner
                if (!ColorsDifferent(B, F) && ColorsDifferent(E, B) && ColorsDifferent(E, C))
                    p3 = Interp2(B, F);

                // Bottom-left corner
                if (!ColorsDifferent(D, H) && ColorsDifferent(E, D) && ColorsDifferent(E, G))
                    p7 = Interp2(D, H);

                // Bottom-right corner
                if (!ColorsDifferent(H, F) && ColorsDifferent(E, H) && ColorsDifferent(E, I))
                    p9 = Interp2(H, F);

                // p2, p4, p5, p6 and p8 stay as E

                // Write 3x3 output block
                S32 dstX = x * 3;
                S32 dstY = y * 3;
                SetPixel(dst, dstWidth, dstX, dstY, p1);
                SetPixel(dst, dstWidth, dstX + 1, dstY, p2);
                SetPixel(dst, dstWidth, dstX + 2, dstY, p3);
                SetPixel(dst, dstWidth, dstX, dstY + 1, p4);
                SetPixel(dst, dstWidth, dstX + 1, dstY + 1, p5);
                SetPixel(dst, dstWidth, dstX + 2, dstY + 1, p6);
                SetPixel(dst, dstWidth, dstX, dstY + 2, p7);
                SetPixel(dst, dstWidth, dstX + 1, dstY + 2, p8);
                SetPixel(dst, dstWidth, dstX + 2, dstY + 2, p9);
            }
        }
    }

    //
    // Generic scale function using current algorithm
    //
    S32 ScaleImage(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst)
    {
        switch (currentAlgorithm)
        {
            case SCALE2X:
                Scale2x(src, srcWidth, srcHeight, dst);
                return 2;

            case SCALE3X:
                Scale3x(src, srcWidth, srcHeight, dst);
                return 3;

            case EAGLE:
                Eagle2x(src, srcWidth, srcHeight, dst);
                return 2;

            case HQ2X:
                Hq2x(src, srcWidth, srcHeight, dst);
                return 2;

            case HQ3X:
                Hq3x(src, srcWidth, srcHeight, dst);
                return 3;

            case NEAREST:
            default:
                Nearest(src, srcWidth, srcHeight, dst, 2);
                return 2;
        }
    }

    //
    // Nearest neighbour at an arbitrary integer factor
    //
    void Nearest(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst, S32 factor)
    {
        ASSERT(factor >= 1)

        S32 dstWidth = srcWidth * factor;

        for (S32 y = 0; y < srcHeight; y++)
        {
            for (S32 x = 0; x < srcWidth; x++)
            {
                U32 P = src[y * srcWidth + x];

                for (S32 sy = 0; sy < factor; sy++)
                {
                    for (S32 sx = 0; sx < factor; sx++)
                    {
                        SetPixel(dst, dstWidth, x * factor + sx, y * factor + sy, P);
                    }
                }
            }
        }
    }

    //
    // Scale by an exact factor, choosing the variant of the current algorithm
    // that natively produces it.
    //
    // The algorithms are not composable in general - running a 2x filter twice
    // is not the same as a 4x filter, and gives the second pass anti-aliased
    // input it was never designed for - so anything without a native variant
    // for the requested factor falls back to nearest neighbour. That is the
    // honest answer: nearest never distorts, it just does not smooth.
    //
    void ScaleImageWith(Algorithm algo, const U32* src, S32 srcWidth, S32 srcHeight,
                        U32* dst, S32 factor)
    {
        ASSERT(factor >= 1)

        if (factor == 1)
        {
            Utils::Memcpy(dst, src, srcWidth * srcHeight * sizeof(U32));
            return;
        }

        switch (algo)
        {
            case SCALE2X:
            case SCALE3X:
                // one family, two factors
                if (factor == 2) { Scale2x(src, srcWidth, srcHeight, dst); return; }
                if (factor == 3) { Scale3x(src, srcWidth, srcHeight, dst); return; }
                break;

            case HQ2X:
            case HQ3X:
                if (factor == 2) { Hq2x(src, srcWidth, srcHeight, dst); return; }
                if (factor == 3) { Hq3x(src, srcWidth, srcHeight, dst); return; }
                break;

            case EAGLE:
                // Eagle has no 3x formulation
                if (factor == 2) { Eagle2x(src, srcWidth, srcHeight, dst); return; }
                break;

            case NEAREST:
            default:
                break;
        }

        Nearest(src, srcWidth, srcHeight, dst, factor);
    }

    //
    // ScaleImageWith using the current general algorithm
    //
    void ScaleImageTo(const U32* src, S32 srcWidth, S32 srcHeight, U32* dst, S32 factor)
    {
        ScaleImageWith(currentAlgorithm, src, srcWidth, srcHeight, dst, factor);
    }

    //
    // Get scaled dimensions based on current algorithm
    //
    S32 GetScaledWidth(S32 srcWidth)
    {
        if (currentAlgorithm == SCALE3X || currentAlgorithm == HQ3X)
            return srcWidth * 3;
        return srcWidth * 2;
    }

    S32 GetScaledHeight(S32 srcHeight)
    {
        if (currentAlgorithm == SCALE3X || currentAlgorithm == HQ3X)
            return srcHeight * 3;
        return srcHeight * 2;
    }

    //
    // Helper: Extract RGBA from a pixel using the bitmap's pixel format
    //
    static inline void ExtractRGBA(U32 pixel, const Pix* pf, S32& r, S32& g, S32& b, S32& a)
    {
        if (pf->rMask)
            r = ((pixel & pf->rMask) >> pf->rShift) << pf->rScaleInv;
        else
            r = 255;

        if (pf->gMask)
            g = ((pixel & pf->gMask) >> pf->gShift) << pf->gScaleInv;
        else
            g = 255;

        if (pf->bMask)
            b = ((pixel & pf->bMask) >> pf->bShift) << pf->bScaleInv;
        else
            b = 255;

        if (pf->aMask)
            a = ((pixel & pf->aMask) >> pf->aShift) << pf->aScaleInv;
        else
            a = 255;  // Fully opaque if no alpha channel
    }

    //
    // Helper: Convert native pixel to canonical ARGB8888 for scaling algorithms
    //
    static inline U32 ToCanonicalARGB(U32 pixel, const Pix* pf)
    {
        S32 r, g, b, a;
        ExtractRGBA(pixel, pf, r, g, b, a);
        return (U32(a) << 24) | (U32(r) << 16) | (U32(g) << 8) | U32(b);
    }

    //
    // Helper: Convert canonical ARGB8888 back to native format
    //
    static inline U32 FromCanonicalARGB(U32 argb, const Pix* pf)
    {
        return pf->MakeRGBA((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
    }

    //
    // Pre-scale an interface texture in place.
    //
    // Returns the factor now applied to the bitmap, which is 1 when nothing was
    // done. The factor is also recorded on the bitmap itself; callers that need
    // it later should read Bitmap::UIScale rather than remembering this.
    //
    // This used to be compiled out behind PIXELSCALE_SCALE2X_UI, because it
    // tracked which bitmaps it had already scaled in a file-static
    // std::set<Bitmap*>. That set was never cleared - ClearScaledBitmaps
    // existed but nothing called it - so it kept raw pointers to bitmaps that
    // could be freed and reallocated at the same address, and it could not see
    // a reload. IFace::OnModeChange reloads every unmanaged bitmap through
    // ReleaseDD + Read, which restores the picture to native size while leaving
    // the set convinced it was still scaled. The next lookup then reported a
    // factor the pixels no longer had.
    //
    // The factor now lives on the Bitmap, so it is reset by whatever replaced
    // the pixels and the question cannot be answered wrongly.
    //
    S32 ScaleBitmapUI(Bitmap* bmp)
    {
        if (!bmp || !GetTextureScaling())
        {
            return 1;
        }

        // already done - and this cannot be stale, see above
        if (bmp->UIScale() > 1)
        {
            return S32(bmp->UIScale());
        }

        S32 factor = GetTextureScale(3);

        if (factor < 2)
        {
            return 1;
        }

        S32 srcWidth = bmp->Width();
        S32 srcHeight = bmp->Height();

        // Skip tiny sources, where there is nothing for an edge filter to read,
        // and ones already large enough that scaling them is mostly cost
        if (srcWidth < 4 || srcHeight < 4 || srcWidth > 512 || srcHeight > 512)
        {
            return 1;
        }

        // Do not exceed what the device will accept. Reducing the factor is
        // better than refusing outright, and 2x is still worth having.
        while (factor > 1
            && ((Vid::caps.maxTexWid && U32(srcWidth * factor) > Vid::caps.maxTexWid)
             || (Vid::caps.maxTexHgt && U32(srcHeight * factor) > Vid::caps.maxTexHgt)))
        {
            factor--;
        }

        if (factor < 2)
        {
            return 1;
        }

        const Pix* pixFormat = bmp->PixelFormat();

        if (!pixFormat)
        {
            return 1;
        }

        // the filters work on canonical ARGB8888, the bitmap may be anything
        U32* srcPixels = new U32[srcWidth * srcHeight];

        bmp->Lock();
        for (S32 y = 0; y < srcHeight; y++)
        {
            for (S32 x = 0; x < srcWidth; x++)
            {
                srcPixels[y * srcWidth + x] = ToCanonicalARGB(bmp->GetPixel(x, y), pixFormat);
            }
        }
        bmp->UnLock();

        S32 dstWidth = srcWidth * factor;
        S32 dstHeight = srcHeight * factor;
        U32* dstPixels = new U32[dstWidth * dstHeight];

        ScaleImageWith(currentAlgorithm, srcPixels, srcWidth, srcHeight, dstPixels, factor);

        // Recreate at the new size, preserving the transparency flags - the old
        // code passed a hardcoded TRUE here, which turned every opaque
        // interface texture translucent
        S32 translucent = bmp->IsTranslucent() ? (bmp->IsTransparent() ? 2 : 1) : 0;

        bmp->Release();
        bmp->Create(dstWidth, dstHeight, translucent);

        // Create may have chosen a different format
        const Pix* newPixFormat = bmp->PixelFormat();

        if (!newPixFormat)
        {
            delete[] srcPixels;
            delete[] dstPixels;
            return 1;
        }

        bmp->Lock();
        for (S32 y = 0; y < dstHeight; y++)
        {
            for (S32 x = 0; x < dstWidth; x++)
            {
                bmp->PutPixel
                (
                    x, y,
                    FromCanonicalARGB(dstPixels[y * dstWidth + x], newPixFormat),
                    &bmp->GetClipRect()
                );
            }
        }
        bmp->UnLock();

        delete[] srcPixels;
        delete[] dstPixels;

        // Create reset this to 1; record what the pixels now are
        bmp->SetUIScale(U32(factor));

        return factor;
    }
}
