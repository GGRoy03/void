static ui_component *
FindComponent(const char *Key)
{
    Cim_Assert(CimCurrent);
    ui_component_hashmap *Hashmap = &UI_LAYOUT.Components;

    if (!Hashmap->IsInitialized)
    {
        Hashmap->GroupCount = 32;

        cim_u32 BucketCount = Hashmap->GroupCount * CimBucketGroupSize;
        size_t  BucketSize = BucketCount * sizeof(ui_component_entry);
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
    cim_u32 Hashed = CimHash_String32(Key);
    cim_u32 GroupIdx = Hashed & (Hashmap->GroupCount - 1);

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
            cim_u32 Idx = (GroupIdx * CimBucketGroupSize) + Lane;

            ui_component_entry *E = Hashmap->Buckets + Idx;
            if (strcmp(E->Key, Key) == 0)
            {
                return &E->Value;
            }

            TagMask &= TagMask - 1;
        }

        __m128i Ev = _mm_set1_epi8(CimEmptyBucketTag);
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

// NOTE: Then do we simply want a helper macro that does this stuff for us user side?
static void
UIRow(void)
{
    ui_layout_node *LayoutNode = PushLayoutNode(true, NULL); Cim_Assert(LayoutNode);
    LayoutNode->Padding       = {5, 5, 5, 5};
    LayoutNode->Spacing       = {10, 0};
    LayoutNode->ContainerType = UIContainer_Row;
}

static void
UIEndRow()
{
    PopParent();
}

static bool
UIWindow(const char *Id, const char *ThemeId, ui_component_state *State)
{
    Cim_Assert(CimCurrent && Id && ThemeId);

    ui_layout_node *LayoutNode = PushLayoutNode(true, State); Cim_Assert(LayoutNode);
    ui_component   *Component  = FindComponent(Id);           Cim_Assert(Component);

    ui_window_theme Theme = GetWindowTheme(ThemeId, Component->ThemeId);
    Cim_Assert(Theme.ThemeId.Value);

    LayoutNode->Padding       = Theme.Padding;
    LayoutNode->Spacing       = Theme.Spacing;
    LayoutNode->ContainerType = UIContainer_Column;

    // TODO: Get the X,Y from somewhere else.
    LayoutNode->X = 500.0f;
    LayoutNode->Y = 400.0f;

    if (Theme.BorderWidth > 0)
    {
        ui_draw_command *Command = AllocateDrawCommand(UIP_LAYOUT.Tree.DrawList);
        Command->Type         = UICommand_Border;
        Command->Pipeline     = UIPipeline_Default;
        Command->ClippingRect = {};
        Command->LayoutNodeId = LayoutNode->Id;

        Command->ExtraData.Border.Color = Theme.BorderColor;
        Command->ExtraData.Border.Width = Theme.BorderWidth;
    }

    {
        ui_draw_command *Command = AllocateDrawCommand(UIP_LAYOUT.Tree.DrawList);
        if (Command)
        {
            Command->Type         = UICommand_Quad;
            Command->Pipeline     = UIPipeline_Default;
            Command->ClippingRect = {};
            Command->LayoutNodeId = LayoutNode->Id;

            Command->ExtraData.Quad.Color = Theme.Color;
        }
    }

    // Cache it for faster retrieval. Could be done at initialization if we have that.
    Component->ThemeId = Theme.ThemeId;

    return true;
}

static void
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

    cim_u32 LayoutNodeId = 0;
    if (LayoutNode)
    {
        LayoutNodeId = LayoutNode->Id;

        // TODO: Idk about this? Probably depends on what the user wants.
        if (LayoutNode->Width < TextContentWidth)
        {
            LayoutNode->Width = TextContentWidth + 10;
        }
    }
    else
    {
        ui_layout_node *Layout = PushLayoutNode(false, NULL); // WARN: Then we are forced to no states on text for now.
        Layout->Width  = TextContentWidth;
        Layout->Height = Font->LineHeight;

        LayoutNodeId = Layout->Id;
    }

    ui_draw_command *Command = AllocateDrawCommand(UIP_LAYOUT.Tree.DrawList);
    if (Command)
    {
        Command->Type         = UICommand_Text;
        Command->ClippingRect = {};
        Command->Pipeline     = UIPipeline_Text;
        Command->LayoutNodeId = LayoutNodeId;

        Command->ExtraData.Text.Font         = Font;
        Command->ExtraData.Text.StringLength = TextLength;
        Command->ExtraData.Text.String       = (char *)Text;     // WARN: Danger!
        Command->ExtraData.Text.Width        = TextContentWidth;
    }
}

static void
ButtonComponent(const char *Id, const char *ThemeId, const char *Text,
                ui_component_state *State, cim_u32 Index)
{
    Cim_Assert(CimCurrent && ThemeId);

    ui_button_theme Theme      = {};
    ui_layout_node *LayoutNode = NULL;
    ui_component   *Component  = NULL;

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

    // NOTE: We could overwrite if the text is bigger and the user wants it.
    LayoutNode = PushLayoutNode(false, State);
    LayoutNode->Width  = Theme.Size.x;
    LayoutNode->Height = Theme.Size.y;

    // Draw Border
    if (Theme.BorderWidth > 0)
    {
        ui_draw_command *Command = AllocateDrawCommand(UIP_LAYOUT.Tree.DrawList);
        Command->Type         = UICommand_Border;
        Command->Pipeline     = UIPipeline_Default;
        Command->ClippingRect = {};
        Command->LayoutNodeId = LayoutNode->Id;

        Command->ExtraData.Border.Color = Theme.BorderColor;
        Command->ExtraData.Border.Width = Theme.BorderWidth;
    }

    // Draw Body
    {
        ui_draw_command *Command = AllocateDrawCommand(UIP_LAYOUT.Tree.DrawList);
        if (Command)
        {
            Command->Type         = UICommand_Quad;
            Command->Pipeline     = UIPipeline_Default;
            Command->ClippingRect = {};
            Command->LayoutNodeId = LayoutNode->Id;

            Command->ExtraData.Quad.Color = Theme.Color;
            Command->ExtraData.Quad.Index = Index;
        }
    }

    // Draw Text
    if (Text)
    {
        TextComponent(Text, LayoutNode, CimCurrent->CurrentFont);
    }
}