// Stand-in for graphics/bitmap.h.
//
// Models the parts of Bitmap that ScaleBitmapUI touches, and - importantly -
// the lifecycle rule the real one now follows: Create and Read replace the
// pixels, so both reset the recorded interface pre-scale factor. That rule is
// what makes the factor impossible to leave stale, and it is what the tests
// here exercise.
#ifndef __STUB_BITMAP_H
#define __STUB_BITMAP_H

#include "utiltypes.h"

struct ClipRect
{
    S32 p0x, p0y, p1x, p1y;
};

//
// 32-bit ARGB only; enough for the format conversion in ScaleBitmapUI
//
struct Pix
{
    U32 rMask, gMask, bMask, aMask;
    U32 rShift, gShift, bShift, aShift;
    U32 rScaleInv, gScaleInv, bScaleInv, aScaleInv;

    Pix()
        : rMask(0x00FF0000), gMask(0x0000FF00), bMask(0x000000FF), aMask(0xFF000000),
          rShift(16), gShift(8), bShift(0), aShift(24),
          rScaleInv(0), gScaleInv(0), bScaleInv(0), aScaleInv(0)
    {
    }

    U32 MakeRGBA(U32 r, U32 g, U32 b, U32 a) const
    {
        return (a << 24) | (r << 16) | (g << 8) | b;
    }
};

class Bitmap
{
public:
    Bitmap(S32 w = 0, S32 h = 0)
        : data(0), uiScale(1), translucent(FALSE), transparent(FALSE),
          bmpWidth(0), bmpHeight(0)
    {
        if (w && h)
        {
            Create(w, h, 0);
        }
    }

    ~Bitmap()
    {
        delete[] data;
    }

    S32 Width() const { return bmpWidth; }
    S32 Height() const { return bmpHeight; }
    F32 InvWidth() const { return bmpWidth ? 1.0f / F32(bmpWidth) : 0.0f; }
    F32 InvHeight() const { return bmpHeight ? 1.0f / F32(bmpHeight) : 0.0f; }

    U32 UIScale() const { return uiScale; }
    void SetUIScale(U32 s) { uiScale = s ? s : 1; }

    S32 UnscaledWidth() const { return bmpWidth / S32(uiScale); }
    S32 UnscaledHeight() const { return bmpHeight / S32(uiScale); }
    F32 InvUnscaledWidth() const { return InvWidth() * F32(uiScale); }
    F32 InvUnscaledHeight() const { return InvHeight() * F32(uiScale); }

    Bool IsTranslucent() { return translucent; }
    Bool IsTransparent() { return transparent; }

    const Pix* PixelFormat() const { return &pixForm; }
    ClipRect& GetClipRect() { return clip; }

    void* Lock() { return data; }
    void UnLock() {}

    U32 GetPixel(S32 x, S32 y) const { return data[y * bmpWidth + x]; }

    void PutPixel(S32 x, S32 y, U32 c, ClipRect*) { data[y * bmpWidth + x] = c; }

    void Release()
    {
        delete[] data;
        data = 0;
    }

    // matches graphics/bitmap.cpp: new pixels, so the recorded pre-scale of
    // the old ones no longer describes them
    Bool Create(S32 w, S32 h, S32 trans)
    {
        uiScale = 1;

        delete[] data;
        bmpWidth = w;
        bmpHeight = h;
        translucent = trans ? TRUE : FALSE;
        transparent = trans > 1 ? TRUE : FALSE;
        data = new U32[w * h];
        for (S32 i = 0; i < w * h; i++)
        {
            data[i] = 0xFF000000;
        }
        return TRUE;
    }

    // matches graphics/bitmap.cpp: a pre-scale has to take its buffer with it,
    // because the readers below only allocate when there is nothing to reuse
    void DropUIScale()
    {
        if (uiScale > 1)
        {
            delete[] data;
            data = 0;
            uiScale = 1;
        }
    }

    // matches Bitmap::Read -> ReadPIC. The important detail is the reuse: when
    // a buffer already exists the reader fills it and leaves the dimensions
    // alone ("read into existing bitmap, truncate bitmap if it will not fit"),
    // so without DropUIScale a pre-scaled bitmap would keep its oversized
    // buffer and its stale factor.
    Bool Read(const char*)
    {
        DropUIScale();

        if (data)
        {
            // reuse - dimensions unchanged
            return TRUE;
        }

        return Create(nativeW, nativeH, translucent ? (transparent ? 2 : 1) : 0);
    }

    // test helper: remember the on-disk size so Read can restore it
    void SetNativeSize(S32 w, S32 h) { nativeW = w; nativeH = h; }

private:
    U32* data;
    U32 uiScale;
    Bool translucent;
    Bool transparent;
    S32 bmpWidth, bmpHeight;
    S32 nativeW, nativeH;
    Pix pixForm;
    ClipRect clip;
};

#endif
