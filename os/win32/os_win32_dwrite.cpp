#include <d2d1.h>
#include <dwrite.h>

static bool
IsValidOSFontContext(os_font_context *Context)
{
    bool Result = (Context) && (Context->TextFormat) && (Context->FontFace) && (Context->RenderTarget) && (Context->FillBrush);
    return Result;
}

static bool
AcquireDirectWrite(byte_string FontName, float FontSize, IDWriteFactory *Factory, IDWriteTextFormat **OutTextFormat, IDWriteFontFace **OutFontFace)
{
    bool Result = 0;

    memory_region Local = EnterMemoryRegion(OSWin32State.Arena);
    wide_string   Name  = ByteStringToWideString(Local.Arena, FontName);

    if(IsValidWideString(Name))
    {
        WCHAR              *FontName = (WCHAR *)Name.String;
        DWRITE_FONT_WEIGHT  Weight   = DWRITE_FONT_WEIGHT_REGULAR;
        DWRITE_FONT_STYLE   Style    = DWRITE_FONT_STYLE_NORMAL;
        DWRITE_FONT_STRETCH Stretch  = DWRITE_FONT_STRETCH_NORMAL;

        Factory->CreateTextFormat(FontName, 0, Weight, Style, Stretch, FontSize, L"en-us", OutTextFormat);

        if(*OutTextFormat)
        {
            (*OutTextFormat)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            (*OutTextFormat)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

            if ((*OutTextFormat))
            {
                IDWriteFontCollection *Collection = 0;
                Factory->GetSystemFontCollection(&Collection);

                if (Collection)
                {
                    UINT32 Index  = 0;
                    BOOL   Exists = false;
                    Collection->FindFamilyName(FontName, &Index, &Exists);

                    if (Exists)
                    {
                        IDWriteFontFamily *Family = 0;
                        Collection->GetFontFamily(Index, &Family);

                        if (Family)
                        {
                            IDWriteFont *DWriteFont = 0;
                            Family->GetFirstMatchingFont(Weight, Stretch, Style, &DWriteFont);
                            if (DWriteFont)
                            {
                                DWriteFont->CreateFontFace(OutFontFace);
                                DWriteFont->Release();
                                Result = (*OutFontFace != 0);
                            }

                            Family->Release();
                        }
                    }

                    Collection->Release();
                }
            }
        }
    }

    LeaveMemoryRegion(Local);

    return Result;
}

static bool
AcquireDirect2D(IDXGISurface *TransferSurface, ID2D1RenderTarget **OutRenderTarget, ID2D1SolidColorBrush **OutColorBrush)
{
    bool Result = 0;

    ID2D1Factory        *D2DFactory = 0;
    D2D1_FACTORY_OPTIONS Options    = { D2D1_DEBUG_LEVEL_ERROR };
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &Options, (void **)&D2DFactory);

    if(D2DFactory)
    {
        D2D1_RENDER_TARGET_PROPERTIES Properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0);

        D2DFactory->CreateDxgiSurfaceRenderTarget(TransferSurface, &Properties, OutRenderTarget);

        if(*OutRenderTarget)
        {
            (*OutRenderTarget)->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), OutColorBrush);
            Result = (*OutRenderTarget) && (*OutColorBrush);
        }

        D2DFactory->Release();
    }

    return Result;
}

static bool
OSAcquireFontContext(byte_string FontName, float FontSize, gpu_font_context *GPUContext, os_font_context *OSContext)
{
    bool Result = 0;

    IDWriteFactory     *Factory       = OSWin32State.DWriteFactory; VOID_ASSERT(Factory);
    IDWriteTextFormat **OutTextFormat = &OSContext->TextFormat;
    IDWriteFontFace   **OutFontFace   = &OSContext->FontFace;

    Result = AcquireDirectWrite(FontName, FontSize, Factory, OutTextFormat, OutFontFace);
    if(Result)
    {
        ID2D1RenderTarget    **OutRenderTarget = &OSContext->RenderTarget;
        ID2D1SolidColorBrush **OutFillBrush    = &OSContext->FillBrush;
        IDXGISurface          *TransferSurface = (IDXGISurface *)GPUContext->GlyphTransferSurface;

        Result = AcquireDirect2D(TransferSurface, OutRenderTarget, OutFillBrush);
    }

    return Result;
}

static void
OSReleaseFontContext(os_font_context *Context)
{
    if(Context)
    {
        if (Context->TextFormat)
        {
            Context->TextFormat->Release();
            Context->TextFormat = 0;
        }

        if(Context->FontFace)
        {
            Context->FontFace->Release();
            Context->FontFace = 0;
        }

        if (Context->FillBrush)
        {
            Context->FillBrush->Release();
            Context->FillBrush = 0;
        }

        if (Context->RenderTarget)
        {
            Context->RenderTarget->Release();
            Context->RenderTarget = 0;
        }
    }
}

static os_glyph_info
OSGetGlyphInfo(byte_string UTF8, float FontSize, os_font_context *Context)
{
    VOID_ASSERT(UTF8.String);
    VOID_ASSERT(UTF8.Size);
    VOID_ASSERT(Context);

    os_glyph_info   Result  = {};
    IDWriteFactory *Factory = OSWin32State.DWriteFactory; VOID_ASSERT(Factory);
    memory_arena   *Arena   = OSWin32State.Arena;         VOID_ASSERT(Arena);

    memory_region Local = EnterMemoryRegion(Arena);

    // BUG: When this fails, the console queue crashes? Why? Wonder if it's a C vs CPP diff?
    if(!IsValidOSFontContext(Context))
    {
        ConsoleWriteMessage(error_message("Calling OSGetGlyphInfo with an invalid font context. See os/core.h"));
        return Result;
    }

    // BUG: When this fails, the console queue crashes? Why? Wonder if it's a C vs CPP diff?
    unicode_decode DecodedUTF8 = DecodeByteString(UTF8.String, UTF8.Size);
    wide_string    UTF16       = ByteStringToWideString(Arena, UTF8);
    if(DecodedUTF8.Codepoint == _UI32_MAX || !IsValidWideString(UTF16))
    {
        ConsoleWriteMessage(error_message("Calling OSGetGlyphInfo with an invalid UTF-8 string. See os/core.h"));
        return Result;
    }

    IDWriteTextLayout *TextLayout = 0;
    {
        WCHAR *String     = (WCHAR*)UTF16.String;
        UINT32 StringSize = (UINT32)UTF16.Size;

        Factory->CreateTextLayout(String, StringSize, Context->TextFormat, 128.f, 128.f, &TextLayout);
    }

    if(TextLayout)
    {
        DWRITE_TEXT_METRICS Metrics = {0};
        TextLayout->GetMetrics(&Metrics);

        uint16_t GlyphIndex = 0;
        uint32_t CodePoint  = DecodedUTF8.Codepoint;

        if(SUCCEEDED(Context->FontFace->GetGlyphIndicesA(&CodePoint, 1, &GlyphIndex)))
        {
            DWRITE_GLYPH_METRICS GlyphMetrics = {0};
            if(SUCCEEDED(Context->FontFace->GetDesignGlyphMetrics(&GlyphIndex, 1, &GlyphMetrics, 0)))
            {
                DWRITE_FONT_METRICS FontMetrics;
                Context->FontFace->GetMetrics(&FontMetrics);

                Result.AdvanceX = (float)(GlyphMetrics.advanceWidth * (FontSize / (float)FontMetrics.designUnitsPerEm) + 0.5f);
                Result.Size.X   = (uint16_t)(Metrics.width  + 0.5f);
                Result.Size.Y   = (uint16_t)(Metrics.height + 0.5f);
                Result.Offset.X = (float)(Metrics.left);
                Result.Offset.Y = (float)(Metrics.top);

                if(Result.Size.X == 0)
                {
                    Result.Size.X = Metrics.widthIncludingTrailingWhitespace;
                }
            }
        }

        TextLayout->Release();
    }

    LeaveMemoryRegion(Local);

    return Result;
}

static bool
OSRasterizeGlyph(byte_string UTF8, rect_float Rect, os_font_context *Context)
{
    bool Result = 0;

    if(!IsValidOSFontContext(Context))
    {
        ConsoleWriteMessage(error_message("Calling OSRasterizeGlyph with an invalid font context. See os/core.h"));
        return Result;
    }

    memory_region Local = EnterMemoryRegion(OSWin32State.Arena);

    unicode_decode DecodedUTF8 = DecodeByteString(UTF8.String, UTF8.Size);
    wide_string    UTF16       = ByteStringToWideString(Local.Arena, UTF8);
    if(DecodedUTF8.Codepoint == _UI32_MAX || !IsValidWideString(UTF16))
    {
        ConsoleWriteMessage(error_message("Calling OSRasterizeGlyph with an invalid UTF-8 string. See os/core.h"));
        return Result;
    }

    LeaveMemoryRegion(Local);

    {
        D2D1_RECT_F DrawRect;
        DrawRect.left   = 0.f;
        DrawRect.top    = 0.f;
        DrawRect.right  = Rect.Right;
        DrawRect.bottom = Rect.Bottom;

        ID2D1RenderTarget    *RenderTarget = Context->RenderTarget;
        IDWriteTextFormat    *TextFormat   = Context->TextFormat;
        ID2D1SolidColorBrush *FillBrush    = Context->FillBrush;

        RenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(D2D1::Size(1.f, 1.f), D2D1::Point2F(0.0f, 0.0f)));
        RenderTarget->BeginDraw();
        RenderTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f));

        WCHAR                 *String  = (WCHAR*)UTF16.String;
        UINT32                 Size    = (UINT32)UTF16.Size;
        D2D1_DRAW_TEXT_OPTIONS Options = D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT;
        DWRITE_MEASURING_MODE  Mode    = DWRITE_MEASURING_MODE_NATURAL;

        RenderTarget->DrawText(String, Size, TextFormat, &DrawRect, FillBrush, Options, Mode);
        Result = SUCCEEDED(RenderTarget->EndDraw());
    }

    return Result;
}

static float
OSGetLineHeight(float FontSize, os_font_context *FontContext)
{
    float Result = 0;

    if(IsValidOSFontContext(FontContext))
    {
        DWRITE_FONT_METRICS FontMetrics = {};
        FontContext->FontFace->GetMetrics(&FontMetrics);

        float Scale  = FontSize / (float)FontMetrics.designUnitsPerEm;
        Result = (float)(FontMetrics.ascent + FontMetrics.descent + FontMetrics.lineGap) * Scale;
    }

    return Result;
}

static void
OSWin32InitializeDWriteFactory(IDWriteFactory **OutFactory)
{
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)OutFactory);
}

