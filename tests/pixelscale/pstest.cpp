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
#include "vid_public.h"

static F32 g_rawScale;

namespace IFace
{
    F32 GetRawScale()
    {
        return g_rawScale;
    }
}

namespace Vid
{
    Caps caps = { 4096, 4096 };
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

    Section("fonts keep their own algorithm");
    {
        SetAlgorithm(NEAREST);
        SetFontAlgorithm(NEAREST);

        // -texup must not reach fonts
        SetAlgorithm(HQ2X);
        Check("texup does not change the font algorithm", GetFontAlgorithm() == NEAREST);

        // and -fontup must not reach textures
        SetFontAlgorithm(SCALE3X);
        Check("fontup does not change the general algorithm", GetAlgorithm() == HQ2X);

        // ScaleImageWith honours the algorithm it is handed, not the global
        U32 src[9] = { K, K, W,
                       K, W, W,
                       W, W, W };
        U32 dst[81], ref[81];

        ScaleImageWith(SCALE3X, src, 3, 3, dst, 3);
        Scale3x(src, 3, 3, ref);
        Check("ScaleImageWith uses its argument", !memcmp(dst, ref, 81 * sizeof(U32)));

        ScaleImageWith(NEAREST, src, 3, 3, dst, 3);
        Nearest(src, 3, 3, ref, 3);
        Check("ScaleImageWith(NEAREST) ignores the global algorithm",
              !memcmp(dst, ref, 81 * sizeof(U32)));

        SetAlgorithm(NEAREST);
        SetFontAlgorithm(NEAREST);
    }

    Section("defaults leave the shipping game alone");
    {
        // Texture pre-scaling must be off until asked for: the filters are
        // selectable so they can be evaluated, not so they change the game.
        // Fonts have been pre-scaled for a while and stay on.
        g_rawScale = 2.0f;
        Init();

        Check("texture scaling defaults off", GetTextureScaling() == FALSE);
        Check("texture scale therefore reports 1", GetTextureScale(4) == 1);
        Check("font scaling defaults on", GetFontScaling() == TRUE);
        Check("font algorithm defaults to nearest", GetFontAlgorithm() == NEAREST);
        Check("layout scaling defaults on", GetUIScaling() == TRUE);
    }

    Section("toggles are independent");
    {
        g_rawScale = 2.75f;

        SetEnabled(TRUE);
        SetUIScaling(TRUE);
        SetTextureScaling(TRUE);
        SetFontScaling(TRUE);
        Init();

        Check("integer scale at 2.75x is 2", GetIntegerScale(4) == 2);
        Check("remainder at 2.75x is 0.75", fabsf(GetScaleRemainder() - 0.75f) < 0.001f);
        Check("the cap argument is honoured", GetIntegerScale(1) == 1);
        Check("texture scale follows", GetTextureScale(4) == 2);
        Check("font scale follows, capped at 3", GetFontScale(3) == 2);

        SetTextureScaling(FALSE);
        Check("texup:off forces texture scale to 1", GetTextureScale(4) == 1);
        Check("texup:off leaves font scale at 2", GetFontScale(3) == 2);
        Check("texup:off leaves layout scaling on", GetUIScaling() == TRUE);

        SetTextureScaling(TRUE);
        SetFontScaling(FALSE);
        Check("fontup:off forces font scale to 1", GetFontScale(3) == 1);
        Check("fontup:off leaves texture scale at 2", GetTextureScale(4) == 2);
        Check("fontup:off leaves layout scaling on", GetUIScaling() == TRUE);

        SetFontScaling(TRUE);
        SetUIScaling(FALSE);
        Check("ui4k:off leaves texture scale at 2", GetTextureScale(4) == 2);
        Check("ui4k:off leaves font scale at 2", GetFontScale(3) == 2);
        Check("ui4k:off reports layout scaling off", GetUIScaling() == FALSE);

        SetUIScaling(TRUE);
        SetEnabled(FALSE);
        Check("upscale:off forces texture scale to 1", GetTextureScale(4) == 1);
        Check("upscale:off forces font scale to 1", GetFontScale(3) == 1);
        Check("upscale:off overrides ui4k:on", GetUIScaling() == FALSE);
        Check("upscale:off overrides texup", GetTextureScaling() == FALSE);
        Check("upscale:off overrides fontup", GetFontScaling() == FALSE);
    }

    Section("texture pre-scaling records its factor on the bitmap");
    {
        g_rawScale = 2.0f;
        SetEnabled(TRUE);
        Init();

        // helper: a 16x16 bitmap with a diagonal in it
        struct Make
        {
            static void Fill(Bitmap& b)
            {
                b.SetNativeSize(16, 16);
                b.Create(16, 16, 1);
                for (S32 y = 0; y < 16; y++)
                    for (S32 x = 0; x < 16; x++)
                        b.PutPixel(x, y, (x > y) ? 0xFFFFFFFF : 0xFF000000, &b.GetClipRect());
            }
        };

        // default: switched off, so nothing happens at all
        SetTextureScaling(FALSE);
        Bitmap off;
        Make::Fill(off);
        Check("does nothing when texture scaling is off", ScaleBitmapUI(&off) == 1);
        Check("and leaves the bitmap at native size", off.Width() == 16);
        Check("and records no factor", off.UIScale() == 1);

        // switched on
        SetTextureScaling(TRUE);
        SetAlgorithm(SCALE2X);
        Bitmap b;
        Make::Fill(b);
        Check("scales when switched on", ScaleBitmapUI(&b) == 2);
        Check("bitmap is now 2x", b.Width() == 32 && b.Height() == 32);
        Check("factor is recorded on the bitmap", b.UIScale() == 2);
        Check("authored size still reported unscaled", b.UnscaledWidth() == 16);
        Check("unscaled UV step matches the authored size",
              fabsf(b.InvUnscaledWidth() - 1.0f / 16.0f) < 0.0001f);

        // asking twice must not scale twice
        Check("second call reports the existing factor", ScaleBitmapUI(&b) == 2);
        Check("and does not scale again", b.Width() == 32);

        // THE regression that disabled this feature, in both of its forms.
        //
        // The old code tracked scaled bitmaps in a side set that no reload
        // could reach, so it went on reporting 2 for a bitmap that was 16
        // wide again. The factor lives with the pixels now.
        //
        // And the readers only allocate when there is no buffer to reuse, so
        // the oversized one has to be dropped with the factor - otherwise the
        // native picture is read into the corner of the larger buffer while
        // the dimensions still claim the scaled size.
        b.Read("whatever.pic");
        Check("a reload drops the recorded factor", b.UIScale() == 1);
        Check("and the bitmap really is native again", b.Width() == 16);
        Check("authored size agrees after the reload", b.UnscaledWidth() == 16);
        Check("and so does the UV step",
              fabsf(b.InvUnscaledWidth() - 1.0f / 16.0f) < 0.0001f);
        Check("so it can be scaled afresh", ScaleBitmapUI(&b) == 2);
        Check("back to 2x", b.Width() == 32 && b.UIScale() == 2);

        // translucency must survive; the old code passed a hardcoded TRUE
        Bitmap opaque;
        opaque.SetNativeSize(16, 16);
        opaque.Create(16, 16, 0);
        ScaleBitmapUI(&opaque);
        Check("an opaque texture stays opaque", opaque.IsTranslucent() == FALSE);

        // device limits reduce the factor rather than refusing outright
        Vid::caps.maxTexWid = Vid::caps.maxTexHgt = 40;
        Bitmap capped;
        Make::Fill(capped);
        S32 f = ScaleBitmapUI(&capped);
        Check("factor is reduced to fit the device limit", f == 2 && capped.Width() == 32);

        Vid::caps.maxTexWid = Vid::caps.maxTexHgt = 20;
        Bitmap tooBig;
        Make::Fill(tooBig);
        Check("and refused when even 2x will not fit", ScaleBitmapUI(&tooBig) == 1);
        Check("leaving it native", tooBig.Width() == 16 && tooBig.UIScale() == 1);

        Vid::caps.maxTexWid = Vid::caps.maxTexHgt = 4096;
        SetTextureScaling(FALSE);
        SetAlgorithm(NEAREST);
    }

    printf("\n%s - %d failure%s\n\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");

    return failures != 0;
}
