// =================================================================
// @Internal: Fonts Implementation

static ui_resource_key
UILoadSystemFont(byte_string Name, float Size, uint16_t CacheSizeX, uint16_t CacheSizeY)
{
    void_context &Context = GetVoidContext();

    ui_resource_key   Key   = MakeFontResourceKey(Name, Size);
    ui_resource_state State = FindResourceByKey(Key, Context.ResourceTable);

    if(!State.Resource)
    {
        uint64_t  Footprint = sizeof(ui_font);
        ui_font  *Font      = static_cast<ui_font *>(malloc(Footprint));

        if(Font)
        {
            ntext::glyph_generator_params GeneratorParams =
            {
                .TextStorage       = ntext::TextStorage::LazyAtlas,
                .FrameMemoryBudget = VOID_MEGABYTE(1),
                .FrameMemory       = malloc(VOID_MEGABYTE(1)),         // Do we really malloc?
                .CacheSizeX        = CacheSizeX,
                .CacheSizeY        = CacheSizeY,
            };

            Font->Generator   = ntext::CreateGlyphGenerator(GeneratorParams);
            Font->Texture     = CreateRenderTexture(CacheSizeX, CacheSizeY, RenderTexture::RGBA);
            Font->TextureView = CreateRenderTextureView(Font->Texture, RenderTexture::RGBA);
            Font->TextureSize = vec2_uint16(CacheSizeX, CacheSizeY);
            Font->Size        = Size;
            Font->Name        = Name;

            UpdateResourceTable(State.Id, Key, Font, Context.ResourceTable);
        }
    }

    return Key;
}

// =================================================================
// @Internal: Static Text Implementation

// TODO:
// So this should be somewhat trivial. We just have to call FillAtlas using the glyph_generator.
// And store the relevant data inside our own structure. We also need to give the rasterized list to
// the renderer.

// TODO:
// On NText side we probably have to provide some kind of source rect instead of simply returning layout info.
// We also have to provide a function that pops the arena every frame.

static uint64_t
GetTextFootprint(uint64_t Size)
{
    return 0;
}


static ui_text *
PlaceTextInMemory(byte_string String, void *Memory)
{
    ui_text *Result = 0;

    if(Memory)
    {
    }

    return Result;
}
