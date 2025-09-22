// [Fonts]

internal ui_font *
UILoadFont(byte_string Name, f32 Size, render_handle BackendHandle, UIFontCoverage_Type Coverage)
{
    ui_font *Result = 0;

    glyph_table_params TableParams = {0};
    {
        TableParams.EntryCount = Coverage == UIFontCoverage_ASCIIOnly ? 255 : 0;
    }

    vec2_i32 TextureSize    = Vec2I32(1024, 1024);
    size_t   Footprint      = sizeof(ui_font);
    size_t   TableFootprint = 0;
    {
        if (Coverage == UIFontCoverage_ASCIIOnly)
        {
            TableFootprint = GetDirectGlyphTableFootprint(TableParams);
        }

        Footprint += TableFootprint;
        Footprint += TextureSize.X * sizeof(stbrp_node);
    }

    memory_arena *Arena = 0;
    {
        memory_arena_params Params = {0};
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.CommitSize        = Footprint;
        Params.ReserveSize       = Footprint;

        Arena = AllocateArena(Params);
    }

    if(Arena)
    {
        b32 IsValid  = 1;
        u8 *HeapBase = PushArena(Arena, Footprint, AlignOf(ui_font));

        if (HeapBase)
        {
            direct_glyph_table *Table = PlaceDirectGlyphTableInMemory(TableParams, HeapBase);

            Result              = (ui_font*)(HeapBase + TableFootprint);
            Result->GlyphTable  = Table;
            Result->TextureSize = Vec2I32ToVec2F32(TextureSize);
            Result->Size        = Size;
            Result->Coverage    = Coverage;
            Result->Arena       = Arena;
            Result->AtlasNodes  = (stbrp_node *)(Result + 1);

            stbrp_init_target(&Result->AtlasContext, TextureSize.X, TextureSize.Y, Result->AtlasNodes, TextureSize.X);
            
            IsValid = CreateGlyphCache(BackendHandle, Result->TextureSize, &Result->GPUFontObjects);
            if (IsValid)
            {
                IsValid = CreateGlyphTransfer(BackendHandle, Result->TextureSize, &Result->GPUFontObjects);
                if (IsValid)
                {
                    IsValid = OSAcquireFontObjects(Name, Size, &Result->GPUFontObjects, &Result->OSFontObjects);
                    if (IsValid)
                    {
                        Result->LineHeight = OSGetLineHeight(Size, &Result->OSFontObjects);
                    }
                    else
                    {
                        OSLogMessage(byte_string_literal("Failed to acquire OS font objects: See error(s) above."), OSMessage_Warn);
                    }
                }
                else
                {
                    OSLogMessage(byte_string_literal("Failed to create glyph transfer: See error(s) above."), OSMessage_Warn);
                }
            }
            else
            {
                OSLogMessage(byte_string_literal("Failed to create glyph cache: See error(s) above."), OSMessage_Warn);
            }
        }

        if (!IsValid)
        {
            ReleaseGlyphCache   (&Result->GPUFontObjects);
            ReleaseGlyphTransfer(&Result->GPUFontObjects);
            OSReleaseFontObjects(&Result->OSFontObjects);

            Result = 0;
        }
    }

    return Result;
}

// [Glyphs]

internal glyph_state
FindGlyphEntryByDirectAccess(u32 CodePoint, direct_glyph_table *Table)
{
    glyph_state Result = {0};

    if (CodePoint < Table->EntryCount)
    {
        Result = Table->Entries[CodePoint];
    }

    return Result;
}

internal size_t
GetDirectGlyphTableFootprint(glyph_table_params Params)
{
    size_t EntryCount = Params.EntryCount * sizeof(glyph_state);
    size_t Result     = EntryCount + sizeof(direct_glyph_table);

    return Result;
}

internal direct_glyph_table *
PlaceDirectGlyphTableInMemory(glyph_table_params Params, void *Memory)
{   Assert(Params.EntryCount > 0 && Memory);

    direct_glyph_table *Result = 0;

    if (Memory)
    {
        glyph_state *Entries = (glyph_state *)Memory;

        Result             = (direct_glyph_table *)Entries + Params.EntryCount;
        Result->Entries    = Entries;
        Result->EntryCount = Params.EntryCount;
    }

    return Result;
}

internal void
UpdateDirectGlyphTableEntry(u32 CodePoint, os_glyph_layout NewLayout, os_glyph_raster_info NewRasterInfo, direct_glyph_table *Table)
{   Assert(Table);

    if (CodePoint < Table->EntryCount)
    {
        glyph_state *State = Table->Entries + CodePoint;
        State->Layout     = NewLayout;
        State->RasterInfo = NewRasterInfo;
    }
}