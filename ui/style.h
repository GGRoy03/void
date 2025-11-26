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
    StyleProperty_CaretColor   = 14,
    StyleProperty_CaretWidth   = 15,

    // Flex Properties
    StyleProperty_FlexDirection  = 16,
    StyleProperty_JustifyContent = 17,
    StyleProperty_AlignItems     = 18,
    StyleProperty_SelfAlign      = 19,
    StyleProperty_FlexGrow       = 20,
    StyleProperty_FlexShrink     = 21,

    StyleProperty_Count        = 22,
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
    bool IsSet;
    bool IsSetRuntime;

    StyleProperty_Type Type;
    union
    {
        uint32_t              Enum;
        float              Float32;
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
    uint32_t            CachedIndex;
} ui_cached_style;

typedef struct ui_style_registry
{
    uint32_t              StylesCount;
    uint32_t              StylesCapacity;
    ui_cached_style *Styles;
} ui_style_registry;

typedef struct ui_node_style
{
    bool             IsLastVersion;
    uint32_t             CachedStyleIndex;
    StyleState_Type State;
    style_property  Properties[StyleState_Count][StyleProperty_Count];
} ui_node_style;

#define UI_PROPERTY_TABLE                                                                \
    X(BorderWidth,    Float32,      float,                   StyleProperty_BorderWidth)    \
    X(Softness,       Float32,      float,                   StyleProperty_Softness)       \
    X(FontSize,       Float32,      float,                   StyleProperty_FontSize)       \
    X(FlexGrow,       Float32,      float,                   StyleProperty_FlexGrow)       \
    X(FlexShrink,     Float32,      float,                   StyleProperty_FlexShrink)     \
    X(CaretWidth,     Float32,      float,                   StyleProperty_CaretWidth)     \
                                                                                         \
    X(CaretColor,     Color,        ui_color,              StyleProperty_CaretColor)     \
    X(Color,          Color,        ui_color,              StyleProperty_Color)          \
    X(BorderColor,    Color,        ui_color,              StyleProperty_BorderColor)    \
    X(TextColor,      Color,        ui_color,              StyleProperty_TextColor)      \
                                                                                         \
    X(Size,           Vec2,         vec2_unit,             StyleProperty_Size)           \
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
static Type UIGet##Name(style_property Properties[StyleProperty_Count])   \
{                                                                           \
    return (Type)Properties[Enum].Member;                                   \
}
UI_PROPERTY_TABLE
#undef X

// NOTE:
// Should just expose a way to clear the paint properties.

// GetHoverStyle:
//

static void             SetNodeStyleState    (StyleState_Type State, uint32_t NodeIndex, ui_subtree *Subtree);
static style_property * GetPaintProperties   (uint32_t NodeIndex, bool ClearState, ui_subtree *Subtree);
static void             SetNodeStyle         (uint32_t NodeIndex, uint32_t Style, ui_subtree *Subtree);
static ui_node_style  * GetNodeStyle         (uint32_t NodeIndex, ui_subtree *Subtree);
static style_property * GetCachedProperties  (uint32_t StyleIndex, StyleState_Type State, ui_style_registry *Registry);

//  Override a style at runtine.

#define UI_STYLE_SETTERS(X) \
    X(Size,      StyleProperty_Size,      Vec2,  vec2_unit) \
    X(Display,   StyleProperty_Display,   Enum,  uint32_t)       \
    X(TextColor, StyleProperty_TextColor, Color, ui_color)  \
    X(Color    , StyleProperty_Color    , Color, ui_color)

#define DEFINE_UI_STYLE_SETTER(Name, PropType, Field, ValueType)                          \
static void                                                                             \
UISet##Name(uint32_t NodeIndex, ValueType Value, ui_subtree *Subtree)                          \
{                                                                                         \
    VOID_ASSERT(Subtree);                                                                      \
    style_property Property = {.Type = PropType, .Field = (ValueType)Value};              \
                                                                                          \
    ui_node_style *NodeStyle = Subtree->ComputedStyles + NodeIndex;                       \
                                                                                          \
    if(NodeStyle)                                                                         \
    {                                                                                     \
        NodeStyle->Properties[StyleState_Default][Property.Type]              = Property; \
        NodeStyle->Properties[StyleState_Default][Property.Type].IsSetRuntime = 1;        \
                                                                                          \
        NodeStyle->IsLastVersion = 0;                                                     \
    }                                                                                     \
}

UI_STYLE_SETTERS(DEFINE_UI_STYLE_SETTER)
#undef DEFINE_UI_STYLE_SETTER

// [Helpers]

static bool IsVisibleColor  (ui_color Color);
