// [Fonts]

static render_handle
GetFontAtlas(ui_font *Font)
{
    render_handle Result = RenderHandle((uint64_t)Font->GPUContext.GlyphCacheView);
    return Result;
}

static ui_font *
UILoadFont(byte_string Name, float Size)
{
    ui_font *Result = 0;

    // Global Access
    render_handle Renderer = RenderState.Renderer;
    memory_arena *Arena    = UIState.StaticData;
    ui_font_list *FontList = &UIState.Fonts;

    if(IsValidByteString(Name) && Size && IsValidRenderHandle(Renderer))
    {
        vec2_int TextureSize = vec2_int(1024, 1024);
        size_t   Footprint   = (TextureSize.X * sizeof(stbrp_node)) + sizeof(ui_font);

        Result = (ui_font *)PushArena(Arena, Footprint, AlignOf(ui_font));

        if(Result)
        {
            Result->Name        = ByteStringCopy(Name, Arena);
            Result->Size        = Size;
            Result->AtlasNodes  = (stbrp_node*)(Result + 1);
            Result->TextureSize = vec2_float((float)TextureSize.X, (float)TextureSize.Y); // NOTE: Weird mismatch.

            stbrp_init_target(&Result->AtlasContext, TextureSize.X, TextureSize.Y, Result->AtlasNodes, TextureSize.X);

            if(CreateGlyphCache(Renderer, Result->TextureSize, &Result->GPUContext))
            {
                if(CreateGlyphTransfer(Renderer, Result->TextureSize, &Result->GPUContext))
                {
                    if(OSAcquireFontContext(Name, Size, &Result->GPUContext, &Result->OSContext))
                    {
                        Result->LineHeight = OSGetLineHeight(Result->Size, &Result->OSContext);
                        AppendToLinkedList(FontList, Result, FontList->Count);
                    }
                }
            }

            // WARN: Wonky
            if(!Result->LineHeight)
            {
                ReleaseGlyphCache(&Result->GPUContext);
                ReleaseGlyphTransfer(&Result->GPUContext);
                OSReleaseFontContext(&Result->OSContext);
            }
        }
        else
        {
        }
    }
    else
    {
    }

    return Result;
}

static ui_font *
UIQueryFont(byte_string FontName, float FontSize)
{
    ui_font *Result = 0;

    ui_font_list *FontList = &UIState.Fonts;
    IterateLinkedList(FontList, ui_font *, Font)
    {
        if (Font->Size == FontSize && ByteStringMatches(Font->Name, FontName, 0))
        {
            Result = Font;
            break;
        }
    }

    return Result;
}

// [Glyphs]

static glyph_entry *
GetGlyphEntry(glyph_table *Table, uint32_t Index)
{
    VOID_ASSERT(Index < Table->EntryCount);
    glyph_entry *Result = Table->Entries + Index;
    return Result;
}

static uint32_t *
GetSlotPointer(glyph_table *Table, glyph_hash Hash)
{
    uint32_t HashIndex = Hash.Value;
    uint32_t HashSlot  = (HashIndex & Table->HashMask);

    VOID_ASSERT(HashSlot < Table->HashCount);
    uint32_t *Result = &Table->HashTable[HashSlot];

    return Result;
}

static glyph_entry *
GetSentinel(glyph_table *Table)
{
    glyph_entry *Result = Table->Entries;
    return Result;
}

static void
UpdateGlyphCacheEntry(glyph_table *Table, glyph_state New)
{
    glyph_entry *Entry = GetGlyphEntry(Table, New.Id);
    if(Entry)
    {
        Entry->IsRasterized = New.IsRasterized;
        Entry->Source       = New.Source;
        Entry->Offset       = New.Offset;
        Entry->AdvanceX     = New.AdvanceX;
        Entry->Position     = New.Position;
        Entry->Size         = New.Size;
    }
}

static bool
IsValidUIText(ui_text *Text)
{
    bool Result = (Text) && (IsValidRenderHandle(Text->Atlas)) && (Text->LineHeight) &&
                 (Text->Shaped) && (Text->ShapedLimit);
    return Result;
}

static ui_shaped_glyph
PrepareGlyph(byte_string UTF8, ui_font *Font)
{
    render_handle Renderer = RenderState.Renderer;
    VOID_ASSERT(IsValidRenderHandle(Renderer));

    glyph_state *State = IsAsciiString(UTF8) ? &Font->Direct[UTF8.String[0]] : 0;
    VOID_ASSERT(State);

    if(!State->IsRasterized)
    {
        os_glyph_info Info = OSGetGlyphInfo(UTF8, Font->Size, &Font->OSContext);

        float Padding     = 2.f;
        float HalfPadding = 1.f;

        stbrp_rect STBRect = {0};
        STBRect.w = (uint16_t)(Info.Size.X + Padding);
        STBRect.h = (uint16_t)(Info.Size.Y + Padding);
        stbrp_pack_rects(&Font->AtlasContext, &STBRect, 1);

        if (STBRect.was_packed)
        {
            rect_float PaddedRect;
            PaddedRect.Left   = (float)STBRect.x + HalfPadding;
            PaddedRect.Top    = (float)STBRect.y + HalfPadding;
            PaddedRect.Right  = (float)(STBRect.x + Info.Size.X + HalfPadding);
            PaddedRect.Bottom = (float)(STBRect.y + Info.Size.Y + HalfPadding);

            State->IsRasterized = OSRasterizeGlyph(UTF8, PaddedRect, &Font->OSContext);
            State->Source       = PaddedRect;
            State->Offset       = Info.Offset;
            State->AdvanceX     = Info.AdvanceX;
            State->Size         = Info.Size;

            TransferGlyph(PaddedRect, Renderer, &Font->GPUContext);
        }
        else
        {
        }
    }

    ui_shaped_glyph Result =
    {
        .Source   = State->Source,
        .Offset   = State->Offset,
        .Size     = State->Size,
        .AdvanceX = State->AdvanceX,
    };

    return Result;
}

// -----------------------------------------------------------------------------------
// Glyph Run Public API Implementation

static uint64_t
GetUITextFootprint(uint64_t TextSize)
{
    uint64_t GlyphBufferSize = TextSize * sizeof(ui_shaped_glyph);
    uint64_t Result          = sizeof(ui_text) + GlyphBufferSize;
    return Result;
}

static void
UITextAppend_(byte_string UTF8, ui_font *Font, ui_text *Text)
{
    if(Text->ShapedCount < Text->ShapedLimit)
    {
        Text->Shaped[Text->ShapedCount++] = PrepareGlyph(UTF8, Font);
    }
}

static void
UITextClear_(ui_text *Text)
{
    VOID_ASSERT(Text);
    Text->ShapedCount = 0;
}

static void
UITextInputMoveCaret_(ui_text *Text, ui_text_input *Input, int Offset)
{
    VOID_ASSERT(Offset != 0);

    int CaretPosition = Input->CaretAnchor + Offset;
    if(CaretPosition >= 0 && CaretPosition <= Text->ShapedCount)
    {
        Input->CaretAnchor = CaretPosition;
        Input->CaretTimer  = 0.f;
    }
}

static void
UITextInputClear_(ui_text_input *TextInput)
{
    VOID_ASSERT(TextInput);
    TextInput->internalCount = 0;
    TextInput->CaretAnchor   = 0;
    MemorySet(TextInput->UserBuffer.String, 0, TextInput->UserBuffer.Size);
}

static ui_text *
PlaceUITextInMemory(byte_string Text, uint64_t BufferSize, ui_font *Font, void *Memory)
{
    VOID_ASSERT(Text.Size <= BufferSize);
    VOID_ASSERT(Font);

    ui_text *Result   = 0;

    if(Memory)
    {
        ui_shaped_glyph *Shaped = (ui_shaped_glyph *)Memory;

        Result = (ui_text *)(Shaped + BufferSize);
        Result->Shaped      = Shaped;
        Result->LineHeight  = Font->LineHeight;
        Result->ShapedCount = 0;
        Result->ShapedLimit = BufferSize;
        Result->Atlas       = GetFontAtlas(Font);
        Result->AtlasSize   = Font->TextureSize;

        while(Result->ShapedCount < Result->ShapedLimit && Text.String && Text.String[Result->ShapedCount])
        {
            // WARN:
            // This decoding looks garbage. And it is.
            byte_string UTF8 = ByteString(&Text.String[Result->ShapedCount], 1);

            Result->Shaped[Result->ShapedCount++] = PrepareGlyph(UTF8, Font);
        }
    }
    else
    {
    }

    return Result;
}
