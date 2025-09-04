static ui_component *
FindComponent(const char *Key)
{
    Cim_Assert(CimCurrent);
    ui_component_hashmap *Hashmap = &UI_Components;

    if (!Hashmap->IsInitialized)
    {
        Hashmap->GroupCount = 32;

        cim_u32 BucketCount  = Hashmap->GroupCount * CimBucketGroupSize;
        size_t  BucketSize   = BucketCount * sizeof(ui_component_entry);
        size_t  MetadataSize = BucketCount * sizeof(cim_u8);

        Hashmap->Buckets = (ui_component_entry *)malloc(BucketSize);
        Hashmap->Metadata = (cim_u8 *)malloc(MetadataSize);

        if (!Hashmap->Buckets || !Hashmap->Metadata)
        {
            return NULL;
        }

        memset(Hashmap->Buckets, 0, BucketSize);
        memset(Hashmap->Metadata, CimEmptyBucketTag, MetadataSize);

        Hashmap->IsInitialized = true;
    }

    cim_u32 ProbeCount = 0;
    cim_u32 Hashed     = CimHash_String32(Key);
    cim_u32 GroupIdx   = Hashed & (Hashmap->GroupCount - 1);

    while (true)
    {
        cim_u8 *Meta = Hashmap->Metadata + GroupIdx * CimBucketGroupSize;
        cim_u8  Tag = (Hashed & 0x7F);

        __m128i Mv = _mm_loadu_si128((__m128i *)Meta);
        __m128i Tv = _mm_set1_epi8(Tag);

        cim_i32 TagMask = _mm_movemask_epi8(_mm_cmpeq_epi8(Mv, Tv));
        while (TagMask)
        {
            cim_u32 Lane = CimHash_FindFirstBit32(TagMask);
            cim_u32 Idx  = (GroupIdx * CimBucketGroupSize) + Lane;

            ui_component_entry *E = Hashmap->Buckets + Idx;
            if (strcmp(E->Key, Key) == 0)
            {
                return &E->Value;
            }

            TagMask &= TagMask - 1;
        }

        __m128i Ev        = _mm_set1_epi8(CimEmptyBucketTag);
        cim_i32 EmptyMask = _mm_movemask_epi8(_mm_cmpeq_epi8(Mv, Ev));
        if (EmptyMask)
        {
            cim_u32 Lane = CimHash_FindFirstBit32(EmptyMask);
            cim_u32 Idx = (GroupIdx * CimBucketGroupSize) + Lane;

            Meta[Lane] = Tag;

            ui_component_entry *E = Hashmap->Buckets + Idx;
            strcpy_s(E->Key, sizeof(E->Key), Key);
            E->Key[sizeof(E->Key) - 1] = 0;

            return &E->Value;
        }

        ProbeCount++;
        GroupIdx = (GroupIdx + ProbeCount * ProbeCount) & (Hashmap->GroupCount - 1);
    }

    return NULL;
}

static void
UIRow(void)
{
    ui_layout_node *LayoutNode = NULL;
    ui_draw_node   *DrawNode   = NULL;
    PushLayoutAndDrawNode(UIContainer_Row, UIDrawNode_None, NULL, &LayoutNode, &DrawNode);

    DrawNode->Pipeline = UIPipeline_None; // NOTE: Should get ignored by the drawer.

    LayoutNode->Padding = {5, 5, 5, 5};
    LayoutNode->Spacing = {10, 0};
}

static void
UIEndRow()
{
    PopParentNode(&UI_LayoutTree);
}

static bool
UIWindow(const char *Id, const char *ThemeId, ui_component_state *State)
{
    Cim_Assert(CimCurrent && Id && ThemeId);

    ui_component *Component = FindComponent(Id);    
    Cim_Assert(Component);

    ui_window_theme Theme = GetWindowTheme(ThemeId, Component->ThemeId);
    Cim_Assert(Theme.ThemeId.Value);

    ui_draw_node   *DrawNode   = NULL;
    ui_layout_node *LayoutNode = NULL;
    PushLayoutAndDrawNode(UIContainer_Column, UIDrawNode_Window, State, &LayoutNode, &DrawNode);

    if (DrawNode && LayoutNode)
    {
        // TODO: Get the X,Y from somewhere else.
        LayoutNode->Padding = Theme.Padding;
        LayoutNode->Spacing = Theme.Spacing;
        LayoutNode->X       = 500.0f;
        LayoutNode->Y       = 400.0f;

        DrawNode->ClippingRect = {};
        DrawNode->Pipeline    = UIPipeline_Default;

        ui_draw_command_border *BorderCommand     = &DrawNode->Data.Window.BorderCommand;
        ui_draw_command_quad   *BackgroundCommand = &DrawNode->Data.Window.BodyCommand;

        if (Theme.BorderWidth > 0)
        {
            BorderCommand->Color     = Theme.BorderColor;
            BorderCommand->Width     = Theme.BorderWidth;
            BorderCommand->IsVisible = true;
        }
        else
        {
            BorderCommand->IsVisible = false;
        }
        
        BackgroundCommand->Color     = Theme.Color;
        BackgroundCommand->IsVisible = true;
    }

    // NOTE: Cache it for next frame.
    Component->ThemeId = Theme.ThemeId;

    return true;
}

static ui_draw_command_text
TextComponent(const char *Text, ui_layout_node *LayoutNode, ui_font *Font)
{
    Cim_Assert(Text);

    cim_u32 TextLength       = (cim_u32)strlen(Text);
    cim_f32 TextContentWidth = 0.0f;

    if (Font->Mode == UIFont_ExtendedASCIIDirectMapping)
    {
        for (cim_u32 Idx = 0; Idx < TextLength; Idx++)
        {
            glyph_atlas AtlasInfo = FindGlyphAtlasByDirectMapping(Font->Table.Direct, Text[Idx]);

            if (!AtlasInfo.IsInAtlas)
            {
                glyph_layout Layout = OSGetGlyphLayout(Text[Idx], Font);

                stbrp_rect Rect = {};
                Rect.w = Layout.Size.Width + 4;
                Rect.h = Layout.Size.Height + 4;
                stbrp_pack_rects(&Font->AtlasContext, &Rect, 1);

                if (Rect.was_packed)
                {
                    ui_texture_coord Tex;
                    Tex.u0 = (cim_f32) Rect.x / Font->AtlasWidth;
                    Tex.v0 = (cim_f32) Rect.y / Font->AtlasHeight;
                    Tex.u1 = (cim_f32)(Rect.x + Rect.w) / Font->AtlasWidth;
                    Tex.v1 = (cim_f32)(Rect.y + Rect.h) / Font->AtlasHeight;

                    OSRasterizeGlyph(Text[Idx], Rect, Font);
                    UpdateDirectGlyphTable(Font->Table.Direct, AtlasInfo.TableId,
                                           true, Tex, Layout.Offsets, Layout.Size,
                                           Layout.AdvanceX);
                }
            }
            else
            {
                glyph_layout Layout = FindGlyphLayoutByDirectMapping(Font->Table.Direct, Text[Idx]);
                TextContentWidth += Layout.AdvanceX;
            }
        }
    }
    else
    {
        // TODO: Other modes.
    }

    ui_draw_command_text Result = {};

    if (LayoutNode)
    {
        Result.Font         = Font;
        Result.String       = (char *)Text;
        Result.StringLength = TextLength;
        Result.Width        = TextContentWidth;
        Result.IsVisible    = true;
    }
    else
    {
        ui_draw_node   *DrawNode   = NULL;
        ui_layout_node *LayoutNode = NULL;
        PushLayoutAndDrawNode(UIContainer_None, UIDrawNode_Text, NULL, &LayoutNode, &DrawNode);

        if (DrawNode && LayoutNode)
        {
            LayoutNode->Width  = TextContentWidth;
            LayoutNode->Height = Font->LineHeight;

            DrawNode->ClippingRect = {};
            DrawNode->Pipeline     = UIPipeline_Text;

            ui_draw_command_text *TextCommand = &DrawNode->Data.Text.TextCommand;
            TextCommand->Font         = Font;
            TextCommand->String       = (char *)Text;
            TextCommand->StringLength = TextLength;
            TextCommand->Width        = TextContentWidth;
            TextCommand->IsVisible    = true;
        }
    }

    return Result;
}

static void
ButtonComponent(const char *Id, const char *ThemeId, const char *Text,
                ui_component_state *State, cim_u32 Index)
{
    Cim_Assert(CimCurrent && ThemeId);

    ui_button_theme Theme     = {};
    ui_component   *Component = NULL;

    if (Id)
    {
        Component = FindComponent(Id); Cim_Assert(Component);
        Theme     = GetButtonTheme(ThemeId, Component->ThemeId);

        // Cache for easier retrieval.
        Component->ThemeId = Theme.ThemeId;
    }
    else
    {
        Theme = GetButtonTheme(ThemeId, {0});
    }

    ui_draw_node   *DrawNode   = NULL;
    ui_layout_node *LayoutNode = NULL;
    PushLayoutAndDrawNode(UIContainer_None, UIDrawNode_Button, State, &LayoutNode, &DrawNode);

    if (DrawNode && LayoutNode)
    {
        LayoutNode->Width  = Theme.Size.x;
        LayoutNode->Height = Theme.Size.y;

        DrawNode->ClippingRect = {};
        DrawNode->Pipeline     = UIPipeline_Default;

        ui_draw_command_border *BorderCommand     = &DrawNode->Data.Button.BorderCommand;
        ui_draw_command_quad   *BackgroundCommand = &DrawNode->Data.Button.BodyCommand;
        ui_draw_command_text   *TextCommand       = &DrawNode->Data.Button.TextCommand;

        if (Theme.BorderWidth > 0)
        {
            BorderCommand->Color     = Theme.BorderColor;
            BorderCommand->Width     = Theme.BorderWidth;
            BorderCommand->IsVisible = true;
        }
        else
        {
            BorderCommand->IsVisible = false;
        }

        BackgroundCommand->Color     = Theme.Color;
        BackgroundCommand->Index     = Index;
        BackgroundCommand->IsVisible = true;

        if (Text)
        {
            *TextCommand = TextComponent(Text, LayoutNode, CimCurrent->CurrentFont);
        }
        else
        {
            TextCommand->IsVisible = false;
        }
    }
}