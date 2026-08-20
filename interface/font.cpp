///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Font system
//
// 29-JAN-1998
//


#include "font.h"
#include "fontsys.h"
#include "filesys.h"
#include "bitmap.h"
#include "vid_public.h"
#include "iface_util.h"
#include "pixelscale.h"


//#define LOG_FONT(x) LOG_DIAG(x)
#define LOG_FONT(x)

// render with trifans instead of bucketing trilists
#define DOTRIFAN

// Gutter around characters
static const S32 Gutter = 1;

// Font texture scaling factor (set during Font::Read based on PixelScale)
static S32 fontTextureScale = 1;


///////////////////////////////////////////////////////////////////////////////
//
// Class Font
//


//
// Font::Font
//
Font::Font()
    : version(0),
      fontHeight(0),
      avgWidth(0)
{
    memset(hashTable, 0, sizeof(hashTable));
}


//
// Font::~Font
//
Font::~Font()
{
    Release();
}


//
// Font::Release
//
void Font::Release()
{
    for (U32 i = 0; i < HASH_COUNT; i++)
    {
        if (hashTable[i].data)
        {
            delete[] hashTable[i].data;
            hashTable[i].data = nullptr;
        }

        hashTable[i].count = 0;
    }
}


//
// Font::FindChar
//
// Find character data for specified character
//
Font::CharData* Font::FindChar(int ch)
{
    U32 key = U32(ch) & HASH_MASK;

    if (hashTable[key].count)
    {
        ASSERT(hashTable[key].data);

        // crappy, replace with binary search
        for (U16 i = 0; i < hashTable[key].count; i++)
        {
            if (hashTable[key].data[i].charValue == ch)
            {
                return (&(hashTable[key].data[i]));
            }
        }
    }
    return (nullptr);
}


//
// Get the font scale factor
// Returns the scale factor used for font texture pre-scaling
// Since textures are pre-scaled, this returns the texture scale factor
// so that Font::Draw() renders at the correct size
//
S32 Font::GetFontScale()
{
    // Return the texture scale factor that was used when loading fonts
    // This ensures Font::Draw() renders glyphs at the correct size
    return fontTextureScale;
}


//
// Return SCALED width of a string
//
S32 Font::Width(const CH* s, S32 len)
{
    // start with zero width (in native pixels)
    S32 width = 0;

    // for each character...
    for (S32 c = 0; c < len; c++)
    {
        if (CharData* cd = FindChar(s[c]))
        {
            // add character width to width
            width += cd->width;
        }
    }

    // return the SCALED width
    return (width * GetFontScale());
}


//
// Return SCALED width of a character
//
S32 Font::Width(S32 c)
{
    if (CharData* cd = FindChar(c))
    {
        return (cd->width * GetFontScale());
    }
    return (0);
}


//
// Return SCALED height of font
//
S32 Font::Height()
{
    return (fontHeight * GetFontScale());
}


//
// Return SCALED average character width
//
S32 Font::AvgWidth()
{
    return (avgWidth * GetFontScale());
}


//
// Font::Read
//
// Reload a font
//
Bool Font::Read()
{
    Release();
    return Read(path.str);
}


//
// Font::Read
//
// Read a font
//
Bool Font::Read(const char* fileName)
{
    Bool rc = FALSE;
    S32 i, x, y;

    // save the file path for mode change reloads
    path = fileName;

    // Clear hash table
    memset(hashTable, 0, sizeof(hashTable));

    // Open the file
    FileSys::DataFile* fp = FileSys::Open(fileName);

    // Did we find the file
    if (fp)
    {
        U8* filePtr = static_cast<U8*>(fp->GetMemoryPtr());

        // Determine version
        U32 fileId = *(U32*)filePtr;
        filePtr += 4;

        if (fileId == FILEID)
        {
            version = 1;
        }
        else if (fileId == FILEID2)
        {
            version = 2;
        }
        else
        {
            ERR_FATAL(("Font file is corrupt [%s]", fileName));
        }

        LOG_FONT(("Reading Font %s:", fileName))
        LOG_FONT((" - Version: %d", version))

        // Version specific header data
        U32 numChar = 0;

        switch (version)
        {
            case 1:
                // In version 1 numchars is 8 bit
                numChar = *static_cast<U8*>(filePtr);
                filePtr += 1;
                break;

            case 2:
                // In version 1 numchars is 16 bit
                numChar = *(U16*)filePtr;
                filePtr += 2;
                break;
        }

        // Common to all versions
        fontHeight = *static_cast<U8*>(filePtr);
        filePtr += 1;

        // Next two members are no longer needed
        filePtr += 2;

        LOG_FONT((" - Chars: %d", numChar))

        // Reset Hash table counters
        U16 tableCount[HASH_COUNT];
        U16 tableBuilt[HASH_COUNT];

        memset(tableCount, 0, sizeof tableCount);
        memset(tableBuilt, 0, sizeof tableBuilt);

        // Read character headers
        CharHeader* charHeaderList = new CharHeader[numChar];

        for (i = 0; i < S32(numChar); i++)
        {
            switch (version)
            {
                case 1:
                    // In version 1 charValue is 8 bits
                    charHeaderList[i].charValue = *static_cast<U8*>(filePtr);
                    filePtr += 1;
                    break;

                case 2:
                    // In version 2 charValue is 8 bits
                    charHeaderList[i].charValue = *(U16*)filePtr;
                    filePtr += 2;
                    break;
            }

            // Common to all versions
            charHeaderList[i].fullWidth = *static_cast<U8*>(filePtr);
            filePtr += 1;
            charHeaderList[i].rectX0 = *static_cast<U8*>(filePtr);
            filePtr += 1;
            charHeaderList[i].rectY0 = *static_cast<U8*>(filePtr);
            filePtr += 1;
            charHeaderList[i].rectX1 = *static_cast<U8*>(filePtr);
            filePtr += 1;
            charHeaderList[i].rectY1 = *static_cast<U8*>(filePtr);
            filePtr += 1;
        }

        // Setup pointers to character data in file
        CharImage** charImageList = new CharImage*[numChar];

        for (i = 0; i < static_cast<S32>(numChar); i++)
        {
            // Setup a pointer to the character data
            charImageList[i] = (CharImage*)filePtr;

            // Sanity check
            if ((charImageList[i]->charWidth > 100) || (charImageList[i]->charHeight > 100))
            {
                ERR_FATAL(("Font file is corrupt [%s]", fileName))
            }

            // Accumulate hash table counters
            U32 key = U32(charHeaderList[i].charValue) & HASH_MASK;

            tableCount[key]++;

            // Advance by header size + data size
            filePtr += sizeof(CharImage) + (charImageList[i]->charWidth * charImageList[i]->charHeight);
        }

        // Allocate hash table
        for (i = 0; i < HASH_COUNT; i++)
        {
            if (tableCount[i])
            {
                hashTable[i].count = tableCount[i];
                hashTable[i].data = new CharData[tableCount[i]];
            }
        }

        // At this point charImageList could be sorted by height to optimize texture construction

        // ...

        // Get font texture scale factor (1, 2, or 3 based on resolution)
        // This pre-scales font textures for crisp rendering at high resolutions
        fontTextureScale = PixelScale::GetIntegerScale(3);
        if (fontTextureScale < 1) fontTextureScale = 1;
        
        LOG_FONT((" - Font texture scale: %d", fontTextureScale));

        // Create textures at scaled size (128 * scale)
        const S32 baseSize = 128;
        const S32 size = baseSize * fontTextureScale;
        const S32 scaledGutter = Gutter * fontTextureScale;

        // Texture handle of current character
        Bitmap* texture = nullptr;
        U16 texHandle = U16_MAX;

        // Texture construction counters (intialized to 0 stop compiler complaining)
        int numTex = 0;
        S32 curX = 0;
        S32 curY = 0;
        S32 rowY = 0;

        // For each character in the font...
        i = 0;

        while (i < S32(numChar))
        {
            // Allocate new texture if necessary
            if (texture == nullptr)
            {
                numTex++;
                texture = FontSys::AllocTexture(texHandle);

                LOG_FONT((" - Allocating texture %d (%dx%d) scale=%d", numTex, size, size, fontTextureScale))

                if (!texture)
                {
                    ERR_FATAL(("Out of font handles!"))
                }

                // Clear the contents (at scaled size)
                texture->Create(size, size, TRUE);
                texture->Clear(0);

                // Lock the texture
                texture->Lock();

                // Reset counters (using scaled gutter)
                curX = scaledGutter;
                curY = scaledGutter;
                rowY = 0;
            }

            // Get file character data structures
            CharHeader& charHeader = charHeaderList[i];
            CharImage* charImage = charImageList[i];

            // Get font character data structure
            U32 hashKey = U32(charHeader.charValue) & HASH_MASK;
            CharData& charData = hashTable[hashKey].data[tableBuilt[hashKey]];

            // If the image has nonzero width...
            if (charImage->charWidth > 0)
            {
                // Calculate scaled character dimensions
                S32 scaledCharWidth = charImage->charWidth * fontTextureScale;
                S32 scaledCharHeight = charImage->charHeight * fontTextureScale;
                
                // If the character won't fit horizontally...
                S32 cw = scaledCharWidth + scaledGutter;
                S32 ch = scaledCharHeight + scaledGutter;

                if (curX + cw > size - scaledGutter)
                {
                    // Do line wrap
                    curX = scaledGutter;
                    curY += rowY;
                    rowY = 0;
                }

                // Update row height
                if (rowY < ch)
                {
                    rowY = ch;
                }

                if (curY + rowY > size - scaledGutter)
                {
                    // Move to next texture
                    texture->UnLock();
                    texture->LoadVideo();
                    texture = nullptr;
                    continue;
                }

                // Get character pixels
                U8* pixel = charImage->charData;

                if (fontTextureScale == 1)
                {
                    // No scaling - write the glyph straight into the atlas
                    for (S32 srcY = 0; srcY < charImage->charHeight; srcY++)
                    {
                        for (S32 srcX = 0; srcX < charImage->charWidth; srcX++)
                        {
                            U8 alpha = *pixel++;

                            texture->PutPixel
                            (
                                curX + srcX, curY + srcY,
                                texture->MakeRGBA(0xFF, 0xFF, 0xFF, alpha),
                                &texture->GetClipRect()
                            );
                        }
                    }
                }
                else
                {
                    // Scale the glyph through the selected algorithm.
                    //
                    // The filters work on 32 bit ARGB, so the glyph is
                    // widened into a scratch buffer first. The source is
                    // white with an 8 bit alpha ramp, which is why the colour
                    // channels are constant here - all the shape lives in
                    // alpha, and that is what the filters end up comparing.
                    const S32 srcPixels = charImage->charWidth * charImage->charHeight;
                    const S32 dstPixels = srcPixels * fontTextureScale * fontTextureScale;

                    U32* srcBuf = new U32[srcPixels];
                    U32* dstBuf = new U32[dstPixels];

                    for (S32 p = 0; p < srcPixels; p++)
                    {
                        srcBuf[p] = 0x00FFFFFF | (U32(*pixel++) << 24);
                    }

                    PixelScale::ScaleImageTo
                    (
                        srcBuf, charImage->charWidth, charImage->charHeight,
                        dstBuf, fontTextureScale
                    );

                    for (S32 dy = 0; dy < scaledCharHeight; dy++)
                    {
                        for (S32 dx = 0; dx < scaledCharWidth; dx++)
                        {
                            // ScaleImageTo works in ARGB; the atlas may not
                            U32 argb = dstBuf[dy * scaledCharWidth + dx];

                            S32 dstX = curX + dx;
                            S32 dstY = curY + dy;

                            ASSERT(dstX < texture->Width());
                            ASSERT(dstY < texture->Height());

                            texture->PutPixel
                            (
                                dstX, dstY,
                                texture->MakeRGBA
                                (
                                    (argb >> 16) & 0xFF,
                                    (argb >> 8) & 0xFF,
                                    argb & 0xFF,
                                    (argb >> 24) & 0xFF
                                ),
                                &texture->GetClipRect()
                            );
                        }
                    }

                    delete[] srcBuf;
                    delete[] dstBuf;
                }

                // Store texture handle
                charData.texHandle = texHandle;

                // Fill in font character data
                // UV coordinates are normalized (0-1) so they work with scaled texture
                float uvScale = 1.0f / size;
                charData.width = charHeader.fullWidth;
                charData.rect.p0.x = charHeader.rectX0;
                charData.rect.p0.y = charHeader.rectY0;
                charData.rect.p1.x = charHeader.rectX1;
                charData.rect.p1.y = charHeader.rectY1;

                charData.u0 = F32(curX) * uvScale + texture->UVShiftWidth();
                charData.v0 = F32(curY) * uvScale + texture->UVShiftHeight();
                charData.u1 = F32(curX + scaledCharWidth) * uvScale + texture->UVShiftWidth();
                charData.v1 = F32(curY + scaledCharHeight) * uvScale + texture->UVShiftHeight();

                // Go to the next position
                curX += cw;
            }
            else
            {
                // Fill in font character data
                charData.width = charHeader.fullWidth;
                charData.rect.p0.x = 0;
                charData.rect.p0.y = 0;
                charData.rect.p1.x = 0;
                charData.rect.p1.y = 0;
                charData.u0 = 0.0f;
                charData.v0 = 0.0f;
                charData.u1 = 0.0f;
                charData.v1 = 0.0f;
                charData.texHandle = 0;
            }

            // Common setup
            charData.charValue = charHeader.charValue;

            // Update built items in the hash table
            tableBuilt[hashKey]++;
            ASSERT(tableBuilt[hashKey] <= tableCount[hashKey]);

            i++;
        }

#ifdef DEVELOPMENT

    // Verify hash table
    for (i = 0; i < HASH_COUNT; i++)
    {
      ASSERT(tableBuilt[i] == tableCount[i]);
    }
    
#endif

        // Unlock the texture
        if (texture)
        {
            texture->UnLock();
            texture->LoadVideo();
        }

        // Setup average character width
        avgWidth = Width(L'A');

        // Delete the character image list
        delete[] charImageList;

        // Delete the character header list
        delete[] charHeaderList;

        // Close the font file
        Close(fp);

        rc = TRUE;
    }

    return rc;
}


//
// Font::Draw
//
// Render the string at scaled size
// x, y are in SCREEN-SPACE (already scaled by caller)
// Glyph positions and sizes are scaled by GetFontScale()
//
void Font::Draw(S32 x, S32 y, const CH* s, U32 len, Color color, const ClipRect* clip, F32 alphaScale, S32 shadow)
{
    F32 fshadow = 0.0F;
    Color shadowClr;
    VertexTL point[4];
    
    // Get integer scale factor for pixel-perfect font rendering
    S32 fontScale = GetFontScale();

    if (shadow)
    {
        shadowClr.Set(0, 0, 0, GetMetric(IFace::SHADOW_ALPHA));
        // Scale shadow offset
        fshadow = F32(shadow * fontScale);
    }

    // Scale the alpha down
    if (alphaScale < 1.0F)
    {
        color.a = U8(Utils::FtoL(F32(color.a) * alphaScale));
    }

    // Clip to screen dimensions if no clip rectangle specified
    if (clip == nullptr)
    {
        clip = &Vid::backBmp.GetClipRect();
    }

    // for each character of the string...
    for (U32 c = 0; c < len; c++)
    {
        // get character data
        if (CharData* cd = FindChar(s[c]))
        {
            // if the character image has nonzero width...
            if (cd->rect.Width())
            {
                Bitmap* texture = FontSys::FindTexture(cd->texHandle);

                // calculate SCALED extents of the character
                // The glyph rect offsets and sizes need to be scaled
                S32 x0 = x + cd->rect.p0.x * fontScale;
                S32 y0 = y + cd->rect.p0.y * fontScale;
                S32 x1 = x + cd->rect.p1.x * fontScale;
                S32 y1 = y + cd->rect.p1.y * fontScale;

                // Clip
                if (x1 > clip->p1.x)
                {
                    return;
                }
                if (x0 >= clip->p0.x)
                {
                    // No filtering, regular clamp mode
                    U16 vertOffset;
                    VertexTL* pointBuf = IFace::GetVerts(4, texture, 0, 0, vertOffset);

                    // Setup indices
                    IFace::SetIndex(Vid::rectIndices, 6, vertOffset);

                    // top left corner
                    point[0].vv.z = 0.0f;
                    point[0].rhw = 1.0f;
                    point[0].diffuse = color;
                    point[0].specular = 0xFF000000;
                    point[0].vv.x = static_cast<F32>(x0);
                    point[0].vv.y = static_cast<F32>(y0);
                    point[0].u = cd->u0;
                    point[0].v = cd->v0;

                    // top right corner
                    point[1].vv.z = 0.0f;
                    point[1].rhw = 1.0f;
                    point[1].diffuse = color;
                    point[1].specular = 0xFF000000;
                    point[1].vv.x = static_cast<F32>(x1);
                    point[1].vv.y = static_cast<F32>(y0);
                    point[1].u = cd->u1;
                    point[1].v = cd->v0;

                    // bottom right corner
                    point[2].vv.z = 0.0f;
                    point[2].rhw = 1.0f;
                    point[2].diffuse = color;
                    point[2].specular = 0xFF000000;
                    point[2].vv.x = static_cast<F32>(x1);
                    point[2].vv.y = static_cast<F32>(y1);
                    point[2].u = cd->u1;
                    point[2].v = cd->v1;

                    // bottom left corner
                    point[3].vv.z = 0.0f;
                    point[3].rhw = 1.0f;
                    point[3].diffuse = color;
                    point[3].specular = 0xFF000000;
                    point[3].vv.x = static_cast<F32>(x0);
                    point[3].vv.y = static_cast<F32>(y1);
                    point[3].u = cd->u0;
                    point[3].v = cd->v1;

                    // Copy vertices into buffer
                    memcpy(pointBuf, point, 4 * sizeof(VertexTL));

                    // Optional shadow
                    if (shadow)
                    {
                        pointBuf = IFace::GetVerts(4, texture, 0, 0, vertOffset);

                        // Setup indices 
                        IFace::SetIndex(Vid::rectIndices, 6, vertOffset);

                        // Modify color and positions for shadow
                        for (U32 i = 0; i < 4; i++)
                        {
                            point[i].vv.x += fshadow;
                            point[i].vv.y += fshadow;
                            point[i].diffuse = shadowClr;
                        }

                        // Copy vertices into buffer
                        memcpy(pointBuf, point, 4 * sizeof(VertexTL));
                    }
                }
            }

            // update position (SCALED character advance)
            x += cd->width * fontScale;
        }
    }
}
