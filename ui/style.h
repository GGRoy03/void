#pragma once

typedef enum StyleProperty_Type
{
    StyleProperty_Size         = 0,
    StyleProperty_Color        = 1,
    StyleProperty_Padding      = 2,
    StyleProperty_Spacing      = 3,
    StyleProperty_Softness     = 4,
    StyleProperty_BorderColor  = 5,
    StyleProperty_BorderWidth  = 6,
    StyleProperty_CornerRadius = 7,
    StyleProperty_Display      = 8,

    // Text Properties
    StyleProperty_Font         = 9,
    StyleProperty_FontSize     = 10,
    StyleProperty_XTextAlign   = 11,
    StyleProperty_YTextAlign   = 12,
    StyleProperty_TextColor    = 13,

    // Flex Properties
    StyleProperty_FlexDirection  = 14,
    StyleProperty_JustifyContent = 15,
    StyleProperty_AlignItems     = 16,
    StyleProperty_SelfAlign      = 17,

    StyleProperty_Count        = 18,
    StyleProperty_None         = 666,
} StyleProperty_Type;

typedef enum StyleState_Type
{
    StyleState_Default = 0,
    StyleState_Hovered = 1,
    StyleState_Focused = 2,

    StyleState_None  = 3,
    StyleState_Count = 3,
} StyleState_Type;

// [CORE TYPES]

#define InvalidCachedStyleIndex 0

typedef struct style_property
{
    b32 IsSet;
    b32 IsSetRuntime;

    StyleProperty_Type Type;
    union
    {
        u32              Enum;
        f32              Float32;
        vec2_unit        Vec2;
        ui_spacing       Spacing;
        ui_padding       Padding;
        ui_color         Color;
        ui_corner_radius CornerRadius;
        void            *Pointer;
        byte_string      String;
    };
} style_property;

typedef struct ui_cached_style
{
    style_property Properties[StyleState_Count][StyleProperty_Count];
    u32            CachedIndex;
} ui_cached_style;

typedef struct ui_style_registry
{
    u32              StylesCount;
    u32              StylesCapacity;
    ui_cached_style *Styles;
} ui_style_registry;

typedef struct ui_node_style
{
    b32             IsLastVersion;
    u32             CachedStyleIndex;
    StyleState_Type State;
    style_property  Properties[StyleState_Count][StyleProperty_Count];
} ui_node_style;

#define UI_PROPERTY_TABLE \
    X(BorderWidth,    Float32,      f32,                   StyleProperty_BorderWidth)    \
    X(Softness,       Float32,      f32,                   StyleProperty_Softness)       \
    X(FontSize,       Float32,      f32,                   StyleProperty_FontSize)       \
    X(Size,           Vec2,         vec2_unit,             StyleProperty_Size)           \
    X(Color,          Color,        ui_color,              StyleProperty_Color)          \
    X(BorderColor,    Color,        ui_color,              StyleProperty_BorderColor)    \
    X(TextColor,      Color,        ui_color,              StyleProperty_TextColor)      \
    X(Padding,        Padding,      ui_padding,            StyleProperty_Padding)        \
    X(Spacing,        Spacing,      ui_spacing,            StyleProperty_Spacing)        \
    X(CornerRadius,   CornerRadius, ui_corner_radius,      StyleProperty_CornerRadius)   \
    X(Font,           Pointer,      ui_font *,             StyleProperty_Font)           \
    X(Display,        Enum,         UIDisplay_Type,        StyleProperty_Display)        \
    X(FlexDirection,  Enum,         UIFlexDirection_Type,  StyleProperty_FlexDirection)  \
    X(JustifyContent, Enum,         UIJustifyContent_Type, StyleProperty_JustifyContent) \
    X(AlignItems,     Enum,         UIAlignItems_Type,     StyleProperty_AlignItems)     \
    X(SelfAlign,      Enum,         UIAlignItems_Type,     StyleProperty_SelfAlign)      \
    X(XTextAlign,     Enum,         UIAlign_Type,          StyleProperty_XTextAlign)     \
    X(YTextAlign,     Enum,         UIAlign_Type,          StyleProperty_YTextAlign)

#define X(Name, Member, Type, Enum)                                         \
internal Type UIGet##Name(style_property Properties[StyleProperty_Count])   \
{                                                                           \
    return (Type)Properties[Enum].Member;                                   \
}
UI_PROPERTY_TABLE
#undef X

// GetHoverStyle:
//

internal void             SetNodeStyleState    (StyleState_Type State, u32 NodeIndex, ui_subtree *Subtree);
internal style_property * GetPaintProperties   (u32 NodeIndex, ui_subtree *Subtree);
internal ui_node_style  * GetNodeStyle         (u32 NodeIndex, ui_subtree *Subtree);
internal style_property * GetCachedProperties  (u32 StyleIndex, StyleState_Type State, ui_style_registry *Registry);

// UISetTextColor:
//  Override a style at runtine.

internal void UISetSize       (u32 NodeIndex, vec2_unit      Size   , ui_subtree *Subtree);
internal void UISetTextColor  (u32 NodeIndex, ui_color       Color  , ui_subtree *Subtree);
internal void UISetDisplay    (u32 NodeIndex, UIDisplay_Type Display, ui_subtree *Subtree);

// [Helpers]

internal b32 IsVisibleColor  (ui_color Color);
