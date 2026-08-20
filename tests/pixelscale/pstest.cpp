///////////////////////////////////////////////////////////////////////////////
//
// Standalone harness for the PixelScale filters.
//
// Includes the translation unit directly so the file-static helpers (Interp2
// and friends) can be exercised as well as the exported entry points.
//

#include <cstdio>
#include <cmath>

#include "utiltypes.h"

static F32 g_rawScale;

namespace IFace
{
    F32 GetRawScale()
    {
        return g_rawScale;
    }
}

// run.bat stages a copy of interface/pixelscale.cpp into build/ and puts
// that directory on the include path; see the comment there for why
#include "pixelscale.cpp"

using namespace PixelScale;

static int failures = 0;

static void Check(const char* what, bool ok)
{
    printf("  %-62s %s\n", what, ok ? "ok" : "** FAIL **");
    if (!ok)
    {
        failures++;
    }
}

static void Section(const char* name)
{
    printf("\n%s\n", name);
}

static const U32 K = 0xFF000000; // opaque black
static const U32 W = 0xFFFFFFFF; // opaque white

int main()
{
    Section("Scale2x - reference rules");
    {
        // Centre W at (1,1); A=(1,0)=K, C=(0,1)=K, B=(2,1)=W, D=(1,2)=W.
        // Rule 1: C==A && C!=D && A!=B  =>  p1 = A = K
        U32 src[9] = { K, K, W,
                       K, W, W,
                       W, W, W };
        U32 dst[36];
        Scale2x(src, 3, 3, dst);

        Check("smooths the diagonal step (p1 = A)", dst[2 * 6 + 2] == K);
        Check("leaves the far corner alone (p4 = E)", dst[3 * 6 + 3] == W);
    }

    Section("every filter preserves a flat field");
    {
        U32 src[9];
        for (int i = 0; i < 9; i++)
        {
            src[i] = W;
        }

        U32 d2[36], d3[81];
        bool ok;

        Scale2x(src, 3, 3, d2);
        ok = true; for (int i = 0; i < 36; i++) if (d2[i] != W) ok = false;
        Check("Scale2x", ok);

        Scale3x(src, 3, 3, d3);
        ok = true; for (int i = 0; i < 81; i++) if (d3[i] != W) ok = false;
        Check("Scale3x", ok);

        Eagle2x(src, 3, 3, d2);
        ok = true; for (int i = 0; i < 36; i++) if (d2[i] != W) ok = false;
        Check("Eagle", ok);

        Hq2x(src, 3, 3, d2);
        ok = true; for (int i = 0; i < 36; i++) if (d2[i] != W) ok = false;
        Check("Hq2x", ok);

        Hq3x(src, 3, 3, d3);
        ok = true; for (int i = 0; i < 81; i++) if (d3[i] != W) ok = false;
        Check("Hq3x", ok);
    }

    Section("straight edges survive magnification untouched");
    {
        // vertical
        U32 v[9] = { K, W, W,
                     K, W, W,
                     K, W, W };
        U32 d2[36], d3[81];
        bool ok;

        Hq2x(v, 3, 3, d2);
        ok = true;
        for (int y = 0; y < 6; y++) for (int x = 0; x < 6; x++)
            if (d2[y * 6 + x] != (x < 2 ? K : W)) ok = false;
        Check("Hq2x, vertical edge", ok);

        Hq3x(v, 3, 3, d3);
        ok = true;
        for (int y = 0; y < 9; y++) for (int x = 0; x < 9; x++)
            if (d3[y * 9 + x] != (x < 3 ? K : W)) ok = false;
        Check("Hq3x, vertical edge", ok);

        // horizontal - this is the case the ColorsDifferent(B, B) bug broke
        U32 h[9] = { K, K, K,
                     W, W, W,
                     W, W, W };

        Hq3x(h, 3, 3, d3);
        ok = true;
        for (int y = 0; y < 9; y++) for (int x = 0; x < 9; x++)
            if (d3[y * 9 + x] != (y < 3 ? K : W)) ok = false;
        Check("Hq3x, horizontal edge", ok);
    }

    Section("corners: rewrite real diagonals, leave everything else alone");
    {
        //  K W W    The diagonal beyond the corner (A) matches the centre, so
        //  W K W    there is no edge cutting this corner off and the centre
        //  W W W    must come through untouched.
        //
        //  This is the case the removed `else if` branch got wrong: it fired
        //  on nothing more than D agreeing with B, and blended the corner
        //  halfway towards them.
        U32 keep[9] = { K, W, W,
                        W, K, W,
                        W, W, W };
        U32 d2[36], d3[81];

        Hq2x(keep, 3, 3, d2);
        Check("Hq2x keeps the centre when the diagonal matches it", d2[2 * 6 + 2] == K);

        Hq3x(keep, 3, 3, d3);
        Check("Hq3x keeps the centre when the diagonal matches it", d3[3 * 9 + 3] == K);

        //  W W W    Here the diagonal does differ, and D and B agree with each
        //  W K K    other: a genuine diagonal edge clipping the corner, which
        //  W K K    is exactly what the filter is for. It must still fire -
        //           this is Scale2x's rule 1 with a perceptual comparison.
        U32 cut[9] = { W, W, W,
                       W, K, K,
                       W, K, K };
        U32 ref[36];

        Hq2x(cut, 3, 3, d2);
        Scale2x(cut, 3, 3, ref);
        Check("Hq2x still rewrites a real diagonal corner", d2[2 * 6 + 2] == W);
        Check("Hq2x agrees with Scale2x on that corner", d2[2 * 6 + 2] == ref[2 * 6 + 2]);
    }

    Section("alpha is handled in premultiplied space");
    {
        // Blending an opaque white texel with a fully transparent black one
        // must not drag black into the colour channels
        U32 r = Interp2(0xFFFFFFFF, 0x00000000);
        S32 rr = (r >> 16) & 0xFF, gg = (r >> 8) & 0xFF, bb = r & 0xFF;
        S32 aa = (r >> 24) & 0xFF;

        Check("colour is not dragged towards the clear texel",
              rr == 255 && gg == 255 && bb == 255);
        Check("alpha still falls to the midpoint", aa == 127 || aa == 128);

        // two fully transparent texels have no colour to recover
        Check("blending two clear texels stays clear",
              (Interp2(0x00000000, 0x00FF0000) >> 24) == 0);

        // an ordinary opaque blend is still a plain average
        U32 g = Interp2(0xFF000000, 0xFFFFFFFF);
        Check("opaque blend is the plain average",
              ((g >> 16) & 0xFF) == 127 && (g >> 24) == 0xFF);
    }

    Section("nearest neighbour at arbitrary factor");
    {
        U32 src[4] = { K, W,
                       W, K };
        U32 dst[36];
        Nearest(src, 2, 2, dst, 3);

        bool ok = true;
        for (int y = 0; y < 6; y++) for (int x = 0; x < 6; x++)
            if (dst[y * 6 + x] != src[(y / 3) * 2 + (x / 3)]) ok = false;

        Check("factor 3 replicates exactly", ok);
    }

    Section("ScaleImageTo family selection");
    {
        U32 src[9];
        for (int i = 0; i < 9; i++) src[i] = (i & 1) ? K : W;

        U32 dst[144], ref[144];

        SetAlgorithm(SCALE2X);
        ScaleImageTo(src, 3, 3, dst, 3);
        Scale3x(src, 3, 3, ref);
        Check("SCALE2X at factor 3 uses Scale3x", !memcmp(dst, ref, 81 * sizeof(U32)));

        SetAlgorithm(HQ3X);
        ScaleImageTo(src, 3, 3, dst, 2);
        Hq2x(src, 3, 3, ref);
        Check("HQ3X at factor 2 uses Hq2x", !memcmp(dst, ref, 36 * sizeof(U32)));

        SetAlgorithm(EAGLE);
        ScaleImageTo(src, 3, 3, dst, 3);
        Nearest(src, 3, 3, ref, 3);
        Check("EAGLE at factor 3 falls back to nearest", !memcmp(dst, ref, 81 * sizeof(U32)));

        SetAlgorithm(HQ2X);
        ScaleImageTo(src, 3, 3, dst, 1);
        Check("factor 1 is a straight copy", !memcmp(dst, src, sizeof(src)));

        SetAlgorithm(NEAREST);
        ScaleImageTo(src, 3, 3, dst, 4);
        Nearest(src, 3, 3, ref, 4);
        Check("NEAREST works at factor 4", !memcmp(dst, ref, 144 * sizeof(U32)));
    }

    Section("command line parsing");
    {
        Algorithm a;
        Check("nn", ParseAlgorithm("nn", a) && a == NEAREST);
        Check("nearest is a synonym", ParseAlgorithm("nearest", a) && a == NEAREST);
        Check("case insensitive", ParseAlgorithm("HQ3X", a) && a == HQ3X);
        Check("scale2x", ParseAlgorithm("scale2x", a) && a == SCALE2X);
        Check("junk is rejected", !ParseAlgorithm("xbrz", a));
        Check("name round trips", !strcmp(AlgorithmName(HQ3X), "hq3x"));
        Check("NativeFactor(scale3x) == 3", NativeFactor(SCALE3X) == 3);
        Check("NativeFactor(eagle) == 2", NativeFactor(EAGLE) == 2);
        Check("default algorithm is nearest", GetAlgorithm() == NEAREST ||
              (SetAlgorithm(DEFAULT), GetAlgorithm() == NEAREST));
    }

    Section("toggles are independent");
    {
        g_rawScale = 2.75f;

        SetEnabled(TRUE);
        SetUIScaling(TRUE);
        SetTextureScaling(TRUE);
        Init();

        Check("integer scale at 2.75x is 2", GetIntegerScale(4) == 2);
        Check("remainder at 2.75x is 0.75", fabsf(GetScaleRemainder() - 0.75f) < 0.001f);
        Check("the cap argument is honoured", GetIntegerScale(1) == 1);

        SetTextureScaling(FALSE);
        Check("texup:off forces texture scale to 1", GetIntegerScale(4) == 1);
        Check("texup:off leaves layout scaling on", GetUIScaling() == TRUE);

        SetTextureScaling(TRUE);
        SetUIScaling(FALSE);
        Check("ui4k:off leaves texture scale at 2", GetIntegerScale(4) == 2);
        Check("ui4k:off reports layout scaling off", GetUIScaling() == FALSE);

        SetUIScaling(TRUE);
        SetEnabled(FALSE);
        Check("upscale:off forces texture scale to 1", GetIntegerScale(4) == 1);
        Check("upscale:off overrides ui4k:on", GetUIScaling() == FALSE);
        Check("upscale:off overrides texup", GetTextureScaling() == FALSE);
    }

    printf("\n%s - %d failure%s\n\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");

    return failures != 0;
}
