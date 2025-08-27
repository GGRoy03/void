typedef enum CimWindow_Flags
{
    CimWindow_AllowDrag = 1 << 0,
} CimWindow_Flags;

#define UIBeginContext() for (UIPass_Type Pass = UIPass_Layout; Pass !=  UIPass_Ended; Pass = (UIPass_Type)((cim_u32)Pass + 1))
#define UIEndContext()   if (Pass == UIPass_Layout) {UIEndLayout();} else if (Pass == UIPass_Draw) {UIEndDraw(UIP_LAYOUT.Tree.DrawList);}

static bool
UILayoutWindow(const char *Id, const char *ThemeId, ui_component_state *State)
{
    // TODO: Think about early exits.
    Cim_Assert(CimCurrent && Id && ThemeId && State);

    ui_layout_node *LayoutNode = PushLayoutNode(true, State);
    Cim_Assert(LayoutNode);

    ui_component *Component = FindComponent(Id);
    Cim_Assert(Component);

    ui_window_theme Theme = GetWindowTheme(ThemeId, Component->ThemeId);
    Cim_Assert(Theme.ThemeId.Value);

    LayoutNode->PrefWidth  = Theme.Size.x;
    LayoutNode->PrefHeight = Theme.Size.y;
    LayoutNode->Padding    = Theme.Padding;
    LayoutNode->Spacing    = Theme.Spacing;

    // TODO: Get the X,Y from somewhere else and do a better job for the layout.
    LayoutNode->X     = 500.0f;
    LayoutNode->Y     = 400.0f;
    LayoutNode->Order = Layout_Horizontal;

    // TODO: Helper?
    ui_draw_info DrawInfo = {};
    DrawInfo.Type         = UICommand_Window;
    DrawInfo.Pipeline     = UIPipeline_Default;
    DrawInfo.ClippingRect = {};
    DrawInfo.LayoutNodeId = LayoutNode->Id;

    Component->ThemeId = Theme.ThemeId;
    DrawInfo.ThemeId   = Theme.ThemeId;

    // TODO: Helper?
    ui_layout_tree *Tree = UIP_LAYOUT.Tree;
    if (Tree->DrawList.CommandCount < Cim_ArrayCount(Tree->DrawList.Commands)) 
    {
        Tree->DrawList.Commands[Tree->DrawList.CommandCount++] = DrawInfo;
    }
    else 
    {
        CimLog_Warn("DrawList overflow while laying out window '%s'", Id);
        return false;
    }

    return true;
}


static void
UILayoutButton(const char *Id, const char *ThemeId, ui_component_state *State)
{
    Cim_Assert(CimCurrent && Id && ThemeId && State);

    ui_layout_node *LayoutNode = PushLayoutNode(false, State);
    Cim_Assert(LayoutNode);

    ui_component *Component = FindComponent(Id);
    Cim_Assert(Component);

    ui_button_theme Theme = GetButtonTheme(ThemeId, Component->ThemeId);
    Cim_Assert(Theme.ThemeId.Value);

    LayoutNode->ContentWidth  = Theme.Size.x;
    LayoutNode->ContentHeight = Theme.Size.y;

    // TODO: Helper?
    ui_draw_info DrawInfo;
    DrawInfo.Type         = UICommand_Button;
    DrawInfo.Pipeline     = UIPipeline_Default;
    DrawInfo.ClippingRect = {};
    DrawInfo.LayoutNodeId = LayoutNode->Id;
    DrawInfo.ThemeId      = Component->ThemeId = Theme.ThemeId;

    // TODO: Helper?
    ui_layout_tree *Tree = UIP_LAYOUT.Tree;
    if (Tree->DrawList.CommandCount < Cim_ArrayCount(Tree->DrawList.Commands))
    {
        Tree->DrawList.Commands[Tree->DrawList.CommandCount++] = DrawInfo;
    }
    else
    {
        CimLog_Warn("DrawList overflow while laying out window '%s'", Id);
    }
}

static void
UIEndLayout()
{
    Cim_Assert(CimCurrent);
    ui_layout_tree *Tree = UIP_LAYOUT.Tree;

    Tree->DragTransformX = 0;
    Tree->DragTransformY = 0;

    cim_u32 DownUpStack[1024] = {};
    cim_u32 StackAt = 0;
    cim_u32 NodeCount = 0;
    char    Visited[512] = {};

    DownUpStack[StackAt++] = 0;

    while (StackAt > 0)
    {
        cim_u32 Current = DownUpStack[StackAt - 1];

        if (!Visited[Current])
        {
            Visited[Current] = 1;

            cim_u32 Child = Tree->Nodes[Current].FirstChild;
            while (Child != CimLayout_InvalidNode)
            {
                DownUpStack[StackAt++] = Child;
                Child = Tree->Nodes[Child].NextSibling;
            }
        }
        else
        {
            StackAt--;
            NodeCount++;

            ui_layout_node *Node = Tree->Nodes + Current;
            if (Node->FirstChild == CimLayout_InvalidNode)
            {
                // Can we set a special flag such that one's width depend on it's 
                // container size? Okay, but then the container's size depends on
                // the content itself.
                Node->PrefWidth = Node->ContentWidth;
                Node->PrefHeight = Node->ContentHeight;
            }
            else
            {
                cim_u32 ChildCount = 0;

                if (Node->Order == Layout_Horizontal)
                {
                    cim_f32 MaximumHeight = 0;
                    cim_f32 TotalWidth = 0;

                    cim_u32 Child = Tree->Nodes[Current].FirstChild;
                    while (Child != CimLayout_InvalidNode)
                    {
                        if (Tree->Nodes[Child].PrefHeight > MaximumHeight)
                        {
                            MaximumHeight = Tree->Nodes[Child].PrefHeight;
                        }

                        TotalWidth += Tree->Nodes[Child].PrefWidth;
                        ChildCount += 1;

                        Child = Tree->Nodes[Child].NextSibling;
                    }

                    cim_f32 Spacing = (ChildCount > 1) ? Node->Spacing.x * (ChildCount - 1) : 0.0f;

                    Node->PrefWidth = TotalWidth + (Node->Padding.x + Node->Padding.z) + Spacing;
                    Node->PrefHeight = MaximumHeight + (Node->Padding.y + Node->Padding.w);
                }
                else if (Node->Order == Layout_Vertical)
                {
                    cim_f32 MaximumWidth = 0;
                    cim_f32 TotalHeight = 0;

                    cim_u32 Child = Tree->Nodes[Current].FirstChild;
                    while (Child != CimLayout_InvalidNode)
                    {
                        if (Tree->Nodes[Child].PrefWidth > MaximumWidth)
                        {
                            MaximumWidth = Tree->Nodes[Child].PrefWidth;
                        }

                        TotalHeight += Tree->Nodes[Child].PrefHeight;
                        ChildCount += 1;

                        Child = Tree->Nodes[Child].NextSibling;
                    }

                    cim_f32 Spacing = (ChildCount > 1) ? Node->Spacing.y * (ChildCount - 1) : 0.0f;

                    Node->PrefWidth = MaximumWidth + (Node->Padding.x + Node->Padding.z);
                    Node->PrefHeight = TotalHeight + (Node->Padding.y + Node->Padding.w) + Spacing;
                }
            }

        }
    }

    cim_u32 UpDownStack[1024] = {};
    StackAt = 0;
    UpDownStack[StackAt++] = 0;

    // How to place the window is still an issue.
    Tree->Nodes[0].Width = Tree->Nodes[0].PrefWidth;
    Tree->Nodes[0].Height = Tree->Nodes[0].PrefHeight;

    cim_f32 ClientX;
    cim_f32 ClientY;

    while (StackAt > 0)
    {
        cim_u32          Current = UpDownStack[--StackAt];
        ui_layout_node *Node = Tree->Nodes + Current;

        ClientX = Node->X + Node->Padding.x;
        ClientY = Node->Y + Node->Padding.y;

        // TODO: Change how we iterate this.
        cim_u32 Child = Node->FirstChild;
        cim_u32 Temp[256] = {};
        cim_u32 Idx = 0;
        while (Child != CimLayout_InvalidNode)
        {
            ui_layout_node *CNode = Tree->Nodes + Child;

            CNode->X = ClientX;
            CNode->Y = ClientY;
            CNode->Width = CNode->PrefWidth;  // Weird.
            CNode->Height = CNode->PrefHeight; // Weird.

            if (Node->Order == Layout_Horizontal)
            {
                ClientX += CNode->Width + Node->Spacing.x;
            }
            else if (Node->Order == Layout_Vertical)
            {
                ClientY += CNode->Height + Node->Spacing.y;
            }

            Temp[Idx++] = Child;
            Child = CNode->NextSibling;
        }

        for (cim_i32 TempIdx = (cim_i32)Idx - 1; TempIdx >= 0; --TempIdx)
        {
            UpDownStack[StackAt++] = Temp[TempIdx];
        }
    }

    PopParent(); // So we can go back to the "root", unsure.

    // NOTE: This is the deep hit-test. Maybe abstract it when reworking?
    bool MouseClicked = IsMouseClicked(CimMouse_Left, UIP_INPUT);

    if (MouseClicked)
    {
        ui_layout_node *Root = Tree->Nodes;
        cim_rect        WindowHitBox = MakeRectFromNode(Root);

        if (IsInsideRect(WindowHitBox))
        {
            for (cim_u32 StackIdx = NodeCount - 1; StackIdx > 0; StackIdx--)
            {
                ui_layout_node *Node = Tree->Nodes + DownUpStack[StackIdx];
                cim_rect        HitBox = MakeRectFromNode(Node);

                if (IsInsideRect(HitBox))
                {
                    Node->State->Clicked = MouseClicked;
                    Node->State->Hovered = true;
                    return;
                }
            }

            Root->State->Hovered = true;
            Root->State->Clicked = MouseClicked;
        }
    }

}

static void
UIEndDraw(ui_draw_list *DrawList)
{
    // WARN: Obviously a lot of duplicate code. Keep this for now.

    for (cim_u32 CommandIdx = 0; CommandIdx < DrawList->CommandCount; CommandIdx++)
    {
        ui_draw_info    *DrawInfo   = DrawList->Commands + CommandIdx;
        ui_layout_node  *LayoutNode = GetUILayoutNode(DrawInfo->LayoutNodeId);
        ui_draw_command *Command    = GetDrawCommand(UIP_COMMANDS, DrawInfo->ClippingRect, DrawInfo->Pipeline);

        switch (DrawInfo->Type)
        {

        case UICommand_Window:
        {
            ui_window_theme Theme = GetWindowTheme(NULL, DrawInfo->ThemeId);

            if (Theme.BorderWidth > 0)
            {
                cim_f32 X0 = LayoutNode->X - Theme.BorderWidth;
                cim_f32 Y0 = LayoutNode->Y - Theme.BorderWidth;
                cim_f32 X1 = LayoutNode->X + LayoutNode->Width  + Theme.BorderWidth;
                cim_f32 Y1 = LayoutNode->Y + LayoutNode->Height + Theme.BorderWidth;

                ui_vertex Borders[4];
                Borders[0] = {X0, Y0, 0.0f, 1.0f, Theme.BorderColor.x, Theme.BorderColor.y, Theme.BorderColor.z, Theme.BorderColor.w};
                Borders[1] = {X0, Y1, 0.0f, 0.0f, Theme.BorderColor.x, Theme.BorderColor.y, Theme.BorderColor.z, Theme.BorderColor.w};
                Borders[2] = {X1, Y0, 1.0f, 1.0f, Theme.BorderColor.x, Theme.BorderColor.y, Theme.BorderColor.z, Theme.BorderColor.w};
                Borders[3] = {X1, Y1, 1.0f, 0.0f, Theme.BorderColor.x, Theme.BorderColor.y, Theme.BorderColor.z, Theme.BorderColor.w};

                cim_u32 Indices[6];
                Indices[0] = Command->VtxCount + 0;
                Indices[1] = Command->VtxCount + 2;
                Indices[2] = Command->VtxCount + 1;
                Indices[3] = Command->VtxCount + 2;
                Indices[4] = Command->VtxCount + 3;
                Indices[5] = Command->VtxCount + 1;

                WriteToArena(Borders, sizeof(Borders), UIP_COMMANDS.FrameVtx);
                WriteToArena(Indices, sizeof(Indices), UIP_COMMANDS.FrameIdx);;

                Command->VtxCount += 4;
                Command->IdxCount += 6;
            }

            cim_f32 X0 = LayoutNode->X;
            cim_f32 Y0 = LayoutNode->Y;
            cim_f32 X1 = LayoutNode->X + LayoutNode->Width;
            cim_f32 Y1 = LayoutNode->Y + LayoutNode->Height;

            ui_vertex Body[4];
            Body[0] = {X0, Y0, 0.0f, 1.0f,  Theme.Color.x, Theme.Color.y, Theme.Color.z, Theme.Color.w};
            Body[1] = {X0, Y1, 0.0f, 0.0f,  Theme.Color.x, Theme.Color.y, Theme.Color.z, Theme.Color.w};
            Body[2] = {X1, Y0, 1.0f, 1.0f,  Theme.Color.x, Theme.Color.y, Theme.Color.z, Theme.Color.w};
            Body[3] = {X1, Y1, 1.0f, 0.0f,  Theme.Color.x, Theme.Color.y, Theme.Color.z, Theme.Color.w};

            cim_u32 Indices[6];
            Indices[0] = Command->VtxCount + 0;
            Indices[1] = Command->VtxCount + 2;
            Indices[2] = Command->VtxCount + 1;
            Indices[3] = Command->VtxCount + 2;
            Indices[4] = Command->VtxCount + 3;
            Indices[5] = Command->VtxCount + 1;

            WriteToArena(Body   , sizeof(Body)   , UIP_COMMANDS.FrameVtx);
            WriteToArena(Indices, sizeof(Indices), UIP_COMMANDS.FrameIdx);;

            Command->VtxCount += 4;
            Command->IdxCount += 6;


        } break;

        case UICommand_Button:
        {
            ui_button_theme Theme = GetButtonTheme(NULL, DrawInfo->ThemeId);

            if (Theme.BorderWidth > 0)
            {
                cim_f32 X0 = LayoutNode->X - Theme.BorderWidth;
                cim_f32 Y0 = LayoutNode->Y - Theme.BorderWidth;
                cim_f32 X1 = LayoutNode->X + LayoutNode->Width  + Theme.BorderWidth;
                cim_f32 Y1 = LayoutNode->Y + LayoutNode->Height + Theme.BorderWidth;

                ui_vertex Borders[4];
                Borders[0] = {X0, Y0, 0.0f, 1.0f, Theme.BorderColor.x, Theme.BorderColor.y, Theme.BorderColor.z, Theme.BorderColor.w};
                Borders[1] = {X0, Y1, 0.0f, 0.0f, Theme.BorderColor.x, Theme.BorderColor.y, Theme.BorderColor.z, Theme.BorderColor.w};
                Borders[2] = {X1, Y0, 1.0f, 1.0f, Theme.BorderColor.x, Theme.BorderColor.y, Theme.BorderColor.z, Theme.BorderColor.w};
                Borders[3] = {X1, Y1, 1.0f, 0.0f, Theme.BorderColor.x, Theme.BorderColor.y, Theme.BorderColor.z, Theme.BorderColor.w};

                cim_u32 Indices[6];
                Indices[0] = Command->VtxCount + 0;
                Indices[1] = Command->VtxCount + 2;
                Indices[2] = Command->VtxCount + 1;
                Indices[3] = Command->VtxCount + 2;
                Indices[4] = Command->VtxCount + 3;
                Indices[5] = Command->VtxCount + 1;

                WriteToArena(Borders, sizeof(Borders), UIP_COMMANDS.FrameVtx);
                WriteToArena(Indices, sizeof(Indices), UIP_COMMANDS.FrameIdx);;

                Command->VtxCount += 4;
                Command->IdxCount += 6;
            }

            cim_f32 X0 = LayoutNode->X;
            cim_f32 Y0 = LayoutNode->Y;
            cim_f32 X1 = LayoutNode->X + LayoutNode->Width;
            cim_f32 Y1 = LayoutNode->Y + LayoutNode->Height;

            ui_vertex Body[4];
            Body[0] = {X0, Y0, 0.0f, 1.0f,  Theme.Color.x, Theme.Color.y, Theme.Color.z, Theme.Color.w};
            Body[1] = {X0, Y1, 0.0f, 0.0f,  Theme.Color.x, Theme.Color.y, Theme.Color.z, Theme.Color.w};
            Body[2] = {X1, Y0, 1.0f, 1.0f,  Theme.Color.x, Theme.Color.y, Theme.Color.z, Theme.Color.w};
            Body[3] = {X1, Y1, 1.0f, 0.0f,  Theme.Color.x, Theme.Color.y, Theme.Color.z, Theme.Color.w};

            cim_u32 Indices[6];
            Indices[0] = Command->VtxCount + 0;
            Indices[1] = Command->VtxCount + 2;
            Indices[2] = Command->VtxCount + 1;
            Indices[3] = Command->VtxCount + 2;
            Indices[4] = Command->VtxCount + 3;
            Indices[5] = Command->VtxCount + 1;

            WriteToArena(Body   , sizeof(Body)   , UIP_COMMANDS.FrameVtx);
            WriteToArena(Indices, sizeof(Indices), UIP_COMMANDS.FrameIdx);;

            Command->VtxCount += 4;
            Command->IdxCount += 6;
        } break;

        }
    }

    DrawList->CommandCount = 0;
}

//// NOTE: We probably don't want to take this as an argument. Something like UISetFont.
//static void
//UITextInternal(const char *TextToRender, UIPass_Type Pass)
//{
//    Cim_Assert(CimCurrent && CimCurrent->CurrentFont.IsValid && TextToRender);
//
//    ui_font Font = CimCurrent->CurrentFont;
//
//    // TODO: Figure this out, because this limits us to one component.
//    static ui_component Component;
//
//    if (Pass == UIPass_Layout)
//    {
//        cim_text *Text = &Component.For.Text;
//
//        // BUG: This is still wrong. Can't we wait on the layout? But the layout on the dimensions.
//        // Weird circular dependency.
//
//        ui_layout_node *Node = PushLayoutNode(false, &Component.LayoutNodeIndex);
//        Node->ContentWidth  = 100;
//        Node->ContentHeight = 50;
//
//        // WARN: I don't know about this.
//        if (!Text->LayoutInfo.BackendLayout)
//        {
//            Text->LayoutInfo = CreateTextLayout((char*)TextToRender, Node->ContentWidth, Node->ContentHeight, Font);
//        }
//
//        // NOTE: Let's do the most naive thing and iterate the string without checking dirty states.
//        for (cim_u32 Idx = 0; Idx < Text->LayoutInfo.GlyphCount; Idx++)
//        {
//            glyph_hash Hash  = ComputeGlyphHash(Text->LayoutInfo.GlyphLayoutInfo[Idx].GlyphId);
//            glyph_info Glyph = FindGlyphEntryByHash(Font.Table, Hash);
//
//            if (!Glyph.IsInAtlas)
//            {
//                glyph_size GlyphSize = GetGlyphExtent((char*) & TextToRender[Idx], 1, Font);
//
//                stbrp_rect Rect;
//                Rect.id = 0;
//                Rect.w  = GlyphSize.Width;
//                Rect.h  = GlyphSize.Height;
//                stbrp_pack_rects(&Font.AtlasContext, &Rect, 1);
//                if (Rect.was_packed)
//                {
//                    cim_f32 U0 = (cim_f32) Rect.x           / Font.AtlasWidth;
//                    cim_f32 V0 = (cim_f32) Rect.y           / Font.AtlasHeight;
//                    cim_f32 U1 = (cim_f32)(Rect.x + Rect.w) / Font.AtlasWidth;
//                    cim_f32 V1 = (cim_f32)(Rect.y + Rect.h) / Font.AtlasHeight;
//
//                    RasterizeGlyph(TextToRender[Idx], Rect, Font);
//                    UpdateGlyphCacheEntry(Font.Table, Glyph.MapId, true, U0, V0, U1, V1, GlyphSize);
//                }
//                else
//                {
//                    // TODO: This either means there is a bug or there is no more
//                    // place in the atlas. Note that we don't have a way to free
//                    // things from the atlas at the moment. I think as things get
//                    // evicted from the cache, we should also free their rects in
//                    // the allocator.
//                }
//            }
//
//            Text->LayoutInfo.GlyphLayoutInfo[Idx].MapId = Glyph.MapId;
//        }
//
//    }
//    else if (Pass == UIPass_User)
//    {
//        // WARN: Then most of this code, has nothing to do on this pass. Which means we now
//        // need 3 pass. Sounds fine to me.
//
//        typedef struct local_vertex
//        {
//            cim_f32 PosX, PosY;
//            cim_f32 U, V;
//        } local_vertex;
//
//        cim_text        *Text = &Component.For.Text;
//        ui_layout_node *Node = GetNodeFromIndex(Component.LayoutNodeIndex); // Can't we just call get next node since it's the same order? Same for hashmap?
//
//        // WARN: Shouldn't be done here.
//        cim_cmd_buffer   *Buffer  = UIP_COMMANDS;
//        cim_draw_command *Command = GetDrawCommand(Buffer, {}, UIPipeline_Text);
//        Command->Font = Font;
//
//        cim_f32 PenX = Node->X;
//        cim_f32 PenY = Node->Y;
//
//        // NOTE: There are two problems currently with this loop:
//        // 1) It's trying to do too much. Move the hashing/checking for atlas to the other phase?
//        // 2) We currently have no way to compute the hash.
//
//        // TODO: Make a draw text routine?
//        for (cim_u32 Idx = 0; Idx < Text->LayoutInfo.GlyphCount; Idx++)
//        {
//            glyph_layout_info *Layout = &Text->LayoutInfo.GlyphLayoutInfo[Idx];
//            glyph_entry       *Glyph  = GetGlyphEntry(Font.Table, Layout->MapId);
//
//            if (PenX + Layout->AdvanceX >= Node->X + Node->Width)
//            {
//                PenX  = Node->X;
//                PenY += Font.LineHeight;
//            }
//
//            cim_f32 MinX = PenX + Layout->OffsetX;
//            cim_f32 MinY = PenY + Layout->OffsetY;
//            cim_f32 MaxX = PenX + Layout->OffsetX + Glyph->Size.Width;
//            cim_f32 MaxY = PenY + Layout->OffsetY + Glyph->Size.Height;
//
//            local_vertex Vtx[4];
//            Vtx[0] = (local_vertex){MinX, MinY, Glyph->U0, Glyph->V0}; // Top-left
//            Vtx[1] = (local_vertex){MinX, MaxY, Glyph->U0, Glyph->V1}; // Bottom-left
//            Vtx[2] = (local_vertex){MaxX, MinY, Glyph->U1, Glyph->V0}; // Top-right
//            Vtx[3] = (local_vertex){MaxX, MaxY, Glyph->U1, Glyph->V1}; // Bottom-right
//
//            cim_u32 Indices[6];
//            Indices[0] = Command->VtxCount + 0;
//            Indices[1] = Command->VtxCount + 2;
//            Indices[2] = Command->VtxCount + 1;
//            Indices[3] = Command->VtxCount + 2;
//            Indices[4] = Command->VtxCount + 3;
//            Indices[5] = Command->VtxCount + 1;
//
//            WriteToArena(Vtx    , sizeof(Vtx)    , &Buffer->FrameVtx);
//            WriteToArena(Indices, sizeof(Indices), &Buffer->FrameIdx);;
//
//            Command->VtxCount += 4;
//            Command->IdxCount += 6;
//
//            PenX += Layout->AdvanceX;
//        }
//
//    }
//}