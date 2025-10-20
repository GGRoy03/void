// [Fonts]

internal render_handle
GetFontAtlas(ui_font *Font)
{
    render_handle Result = RenderHandle((u64)Font->GPUContext.GlyphCacheView);
    return Result;
}

internal ui_font *
UILoadFont(byte_string Name, f32 Size)
{
    ui_font *Result = 0;

    // Global Access
    render_handle Renderer = RenderState.Renderer;
    memory_arena *Arena    = UIState.StaticData;
    ui_font_list *FontList = &UIState.Fonts;

    if(IsValidByteString(Name) && Size && IsValidRenderHandle(Renderer))
    {
        vec2_i32 TextureSize = Vec2I32(1024, 1024);
        size_t   Footprint   = (TextureSize.X * sizeof(stbrp_node)) + sizeof(ui_font);

        Result = PushArena(Arena, Footprint, AlignOf(ui_font));

        if(Result)
        {
            Result->Name        = ByteStringCopy(Name, Arena);
            Result->Size        = Size;
            Result->AtlasNodes  = (stbrp_node*)(Result + 1);
            Result->TextureSize = Vec2I32ToVec2F32(TextureSize);

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
            ConsoleWriteMessage(error_message("Failed to allocate memory when calling UILoadFont."));
        }
    }
    else
    {
        ConsoleWriteMessage(error_message("One or more arguments given to UILoadFont is invalid."));
    }

    return Result;
}

internal ui_font *
UIQueryFont(byte_string FontName, f32 FontSize)
{
    ui_font *Result = 0;

    ui_font_list *FontList = &UIState.Fonts;
    IterateLinkedList(FontList, ui_font *, Font)
    {
        if (Font->Size == FontSize && ByteStringMatches(Font->Name, FontName, NoFlag))
        {
            Result = Font;
            break;
        }
    }

    return Result;
}

// [Glyphs]

internal glyph_entry *
GetGlyphEntry(glyph_table *Table, u32 Index)
{
    Assert(Index < Table->EntryCount);
    glyph_entry *Result = Table->Entries + Index;
    return Result;
}

internal u32 *
GetSlotPointer(glyph_table *Table, glyph_hash Hash)
{
    u32 HashIndex = Hash.Value;
    u32 HashSlot  = (HashIndex & Table->HashMask);

    Assert(HashSlot < Table->HashCount);
    u32 *Result = &Table->HashTable[HashSlot];

    return Result;
}

internal glyph_entry *
GetSentinel(glyph_table *Table)
{
    glyph_entry *Result = Table->Entries;
    return Result;
}

internal void
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

// WARN: Known limitations:
// 1) Only handles 1 byte UTF8 encoding
// 2) Only handles direct glyph access

internal ui_glyph_run
CreateGlyphRun(byte_string Text, ui_font *Font, memory_arena *Arena)
{
    ui_glyph_run Result = {0};

    // Global Access
    render_handle Renderer = RenderState.Renderer;

    if(IsValidByteString(Text) && IsValidRenderHandle(Renderer) && Arena)
    {
        Result.Glyphs     = PushArray(Arena, ui_glyph, Text.Size);
        Result.LineHeight = Font->LineHeight;
        Result.GlyphCount = Text.Size;
        Result.Atlas      = GetFontAtlas(Font);
        Result.AtlasSize  = Font->TextureSize;

        for(u32 Idx = 0; Idx < Text.Size; ++Idx)
        {
            // NOTE: What kind of dogshit is this?

            byte_string UTF8Stream = ByteString(&Text.String[Idx], 1);

            glyph_state *State = 0;
            if(IsAsciiString(UTF8Stream))
            {
                State = &Font->Direct[Text.String[Idx]];
            }
            else
            {
                // TODO: Call the cache..
                return Result;
            }

            if(!State->IsRasterized)
            {
                os_glyph_info Info = OSGetGlyphInfo(UTF8Stream, Font->Size, &Font->OSContext);
                f32           Padding     = 2.f;
                f32           HalfPadding = 1.f;

                stbrp_rect STBRect = { 0 };
                STBRect.w = (u16)(Info.Size.X + Padding);
                STBRect.h = (u16)(Info.Size.Y + Padding);
                stbrp_pack_rects(&Font->AtlasContext, &STBRect, 1);

                if (STBRect.was_packed)
                {
                    rect_f32 PaddedRect;
                    PaddedRect.Min.X = (f32)STBRect.x + HalfPadding;
                    PaddedRect.Min.Y = (f32)STBRect.y + HalfPadding;
                    PaddedRect.Max.X = (f32)(STBRect.x + Info.Size.X + HalfPadding);
                    PaddedRect.Max.Y = (f32)(STBRect.y + Info.Size.Y + HalfPadding);

                    State->IsRasterized = OSRasterizeGlyph(UTF8Stream, PaddedRect, &Font->OSContext);
                    State->Source.Min.X = PaddedRect.Min.X;
                    State->Source.Min.Y = PaddedRect.Min.Y;
                    State->Source.Max.X = PaddedRect.Max.X;
                    State->Source.Max.Y = PaddedRect.Max.Y;
                    State->Offset       = Info.Offset;
                    State->AdvanceX     = Info.AdvanceX;
                    State->Size         = Info.Size;

                    TransferGlyph(PaddedRect, Renderer, &Font->GPUContext);
                }
                else
                {
                    ConsoleWriteMessage(warn_message("Could not pack glyph inside the texture in CreateGlyphRun"));
                }
            }

            Result.Glyphs[Idx].Source   = State->Source;
            Result.Glyphs[Idx].Offset   = State->Offset;
            Result.Glyphs[Idx].AdvanceX = State->AdvanceX;
            Result.Glyphs[Idx].Size     = State->Size;
        }
    }
    else
    {
        ConsoleWriteMessage(warn_message("One or more arguments passed to CreateGlyphRun is invalid"));
    }

    return Result;
}
