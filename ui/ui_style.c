internal void
SetLayoutNodeStyle(ui_cached_style *CachedStyle, ui_layout_node *Node, bit_field Flags)
{
    if (IsValidLayoutNode(Node) && CachedStyle)
    {
        Node->CachedStyle = CachedStyle;

        Node->Value.Width   = CachedStyle->Value.Size.X;
        Node->Value.Height  = CachedStyle->Value.Size.Y;
        Node->Value.Padding = CachedStyle->Value.Padding;
        Node->Value.Spacing = CachedStyle->Value.Spacing;

        if (!HasFlag(Flags, SetLayoutNodeStyle_OmitReference))
        {
            AppendToLinkedList(CachedStyle, Node, CachedStyle->RefCount);
        }
    }
}

internal ui_cached_style *
UIGetStyleSentinel(byte_string Name, ui_style_subregistry *Registry)
{
    ui_cached_style *Result = 0;

    if (Name.Size)
    {
        Result = Registry->Sentinels + (Name.Size - 1);
    }

    return Result;
}

internal ui_style_name
UIGetCachedName(byte_string Name, ui_style_registry *Registry)
{
    // WARN: This is terrible?

    ui_style_name Result = {0};

    if (Name.Size)
    {
        IterateLinkedList(Registry->First, ui_style_subregistry *, Sub)
        {
            ui_cached_style *Sentinel = UIGetStyleSentinel(Name, Sub);
            IterateLinkedList(Sentinel->Next, ui_cached_style *, CachedStyle)
            {
                byte_string CachedString = Sub->CachedNames[CachedStyle->Index].Value;
                if (ByteStringMatches(Name, CachedString, StringMatch_CaseSensitive))
                {
                    Result = Sub->CachedNames[CachedStyle->Index];
                    break;
                }
            }
        }
    }

    return Result;
}

internal ui_style_name
UIGetCachedNameFromSubregistry(byte_string Name, ui_style_subregistry *Registry)
{
    ui_style_name Result = { 0 };

    if (Name.Size)
    {
        ui_cached_style *Sentinel = UIGetStyleSentinel(Name, Registry);
        IterateLinkedList(Sentinel->Next, ui_cached_style *, CachedStyle)
        {
            byte_string CachedString = Registry->CachedNames[CachedStyle->Index].Value;
            if (ByteStringMatches(Name, CachedString, StringMatch_CaseSensitive))
            {
                Result = Registry->CachedNames[CachedStyle->Index];
                break;
            }
        }
    }

    return Result;
}

internal ui_cached_style *
UIGetStyle(ui_style_name Name, ui_style_registry *Registry)
{
    ui_cached_style *Result = 0;

    if (Registry)
    {
        IterateLinkedList(Registry->First, ui_style_subregistry *, Sub)
        {
            ui_cached_style *Sentinel = UIGetStyleSentinel(Name.Value, Sub);
            if (Sentinel)
            {
                IterateLinkedList(Sentinel->Next, ui_cached_style *, CachedStyle)
                {
                    byte_string CachedString = Sub->CachedNames[CachedStyle->Index].Value;
                    if (Name.Value.String == CachedString.String)
                    {
                        Result = CachedStyle;
                        break;
                    }
                }
            }
            else
            {
            }
        }
    }

    return Result;
}

internal ui_cached_style *
UIGetStyleFromSubregistry(ui_style_name Name, ui_style_subregistry *Registry)
{
    ui_cached_style *Result = 0;

    if (Registry)
    {
        ui_cached_style *Sentinel = UIGetStyleSentinel(Name.Value, Registry);
        if (Sentinel)
        {
            IterateLinkedList(Sentinel->Next, ui_cached_style *, CachedStyle)
            {
                byte_string CachedString = Registry->CachedNames[CachedStyle->Index].Value;
                if (Name.Value.String == CachedString.String)
                {
                    Result = CachedStyle;
                    break;
                }
            }
        }
    }

    return Result;
}